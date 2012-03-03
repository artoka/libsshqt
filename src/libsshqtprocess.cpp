
#include <QDebug>
#include <QMetaEnum>
#include <QProcessEnvironment>
#include <QCoreApplication>
#include <QUrl>

#include "libsshqtprocess.h"
#include "libsshqtclient.h"
#include "libsshqtdebug.h"



//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// LibsshQtProcess
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

LibsshQtProcess::LibsshQtProcess(LibsshQtClient *parent) :
    LibsshQtChannel(parent),
    state_(StateClosed),
    exit_code_(-1),
    stderr_(new LibsshQtProcessStderr(this))
{
    debug_prefix_ = LibsshQt::debugPrefix(this);
    stderr_->err_buffer_ = &stderr_buffer_;

    LIBSSHQT_DEBUG("Constructor");

    timer_.setSingleShot(true);
    timer_.setInterval(0);

    connect(&timer_, SIGNAL(timeout()),        this, SLOT(processState()));
    connect(parent,  SIGNAL(error()),          this, SLOT(handleClientError()));
    connect(parent,  SIGNAL(doProcessState()), this, SLOT(processState()));
    connect(parent,  SIGNAL(doCleanup()),      this, SLOT(closeChannel()));

    setStdoutBehaviour(OutputToQDebug, "Remote stdout:");
    setStderrBehaviour(OutputToQDebug, "Remote stderr:");
}

LibsshQtProcess::~LibsshQtProcess()
{
    LIBSSHQT_DEBUG("Destructor");
    closeChannel();
}

const char *LibsshQtProcess::enumToString(const State value)
{
    return staticMetaObject.enumerator(
                staticMetaObject.indexOfEnumerator("State"))
                    .valueToKey(value);
}

const char *LibsshQtProcess::enumToString(const OutputBehaviour value)
{
    return staticMetaObject.enumerator(
                staticMetaObject.indexOfEnumerator("OutputBehaviour"))
                    .valueToKey(value);
}

void LibsshQtProcess::setCommand(QString command)
{
    command_ = command;
    LIBSSHQT_DEBUG("Setting command to" << command);
}

QString LibsshQtProcess::command()
{
    return command_;
}

int LibsshQtProcess::exitCode() const
{
    return exit_code_;
}

void LibsshQtProcess::setStdoutBehaviour(OutputBehaviour behaviour,
                                         QString         prefix)
{
    LIBSSHQT_DEBUG("Setting stdout behaviour to" << behaviour);

    stdout_behaviour_     = behaviour;
    stdout_output_prefix_ = prefix;

    if ( behaviour == OutputManual ) {
        disconnect(this, SIGNAL(readyRead()),
                   this, SLOT(handleStdoutOutput()));

    } else {
        connect(this, SIGNAL(readyRead()),
                this, SLOT(handleStdoutOutput()),
                Qt::UniqueConnection);
        handleStdoutOutput();
    }
}

void LibsshQtProcess::setStderrBehaviour(OutputBehaviour behaviour,
                                         QString         prefix)
{
    LIBSSHQT_DEBUG("Setting stderr behaviour to" << behaviour);

    stderr_behaviour_     = behaviour;
    stderr_output_prefix_ = prefix;

    if ( behaviour == OutputManual ) {
        disconnect(stderr_, SIGNAL(readyRead()),
                   this, SLOT(handleStderrOutput()));

    } else {
        connect(stderr_, SIGNAL(readyRead()),
                this, SLOT(handleStderrOutput()),
                Qt::UniqueConnection);
        handleStderrOutput();
    }
}

LibsshQtProcessStderr *LibsshQtProcess::stderr()
{
    return stderr_;
}

void LibsshQtProcess::openChannel()
{
    if ( state_ == StateClosed ) {
        setState(StateWaitClient);
        timer_.start();
    }
}

void LibsshQtProcess::closeChannel()
{
    if ( state_ != StateClosed &&
         state_ != StateClosing ){

        // Prevent recursion
        setState(StateClosing);

        emit readChannelFinished();
        handleStdoutOutput();
        handleStderrOutput();

        close();
        stderr_->close();
        setState(StateClosed);
    }
}

LibsshQtProcess::State LibsshQtProcess::state() const
{
    return state_;
}

void LibsshQtProcess::setState(State state)
{
    if ( state_ == state ) {
        LIBSSHQT_DEBUG("State is already" << state);
        return;
    }

    LIBSSHQT_DEBUG("Changing state to" << state);
    state_ = state;

    switch (  state_ ) {
    case StateClosed:       emit closed();          break;
    case StateClosing:                              break;
    case StateWaitClient:                           break;
    case StateOpening:                              break;
    case StateExec:                                 break;
    case StateOpen:         emit opened();          break;
    case StateError:        emit error();           break;
    case StateClientError:  emit error();           break;
    }
}

void LibsshQtProcess::processState()
{
    switch ( state_ ) {

    case StateClosed:
    case StateClosing:
    case StateError:
    case StateClientError:
        return;

    case StateWaitClient:
    {
        if ( client_->state() == LibsshQtClient::StateOpened ) {
            setState(StateOpening);
            timer_.start();
        }
        return;
    } break;

    case StateOpening:
    {
        if ( ! channel_ ) {
            channel_ = ssh_channel_new(client_->sshSession());
            if ( ! channel_ ) {
                LIBSSHQT_FATAL("Could not create SSH channel");
            }
        }

        int rc = ssh_channel_open_session(channel_);

        switch ( rc ) {
        case SSH_AGAIN:
            client_->enableWritableNotifier();
            return;

        case SSH_ERROR:
            LIBSSHQT_DEBUG("Channel open error:" << errorCodeAndMessage());
            setState(StateError);
            return;

        case SSH_OK:
            setState(StateExec);
            timer_.start();
            return;

        default:
            LIBSSHQT_CRITICAL("Unknown result code" << rc <<
                              "received from ssh_channel_open_session()");
            return;
        }
    } break;

    case StateExec:
    {
        int rc = ssh_channel_request_exec(channel_, qPrintable(command_));

        switch ( rc ) {
        case SSH_AGAIN:
            client_->enableWritableNotifier();
            return;

        case SSH_ERROR:
            LIBSSHQT_DEBUG("Channel exec error:" << errorCodeAndMessage());
            setState(StateError);
            return;

        case SSH_OK:
            LibsshQtChannel::open();
            stderr_->open();
            setState(StateOpen);
            timer_.start();
            return;

        default:
            LIBSSHQT_CRITICAL("Unknown result code" << rc <<
                              "received from ssh_channel_request_exec()");
            return;
        }
    } break;

    case StateOpen:
    {
        checkIo();

        if ( state_ == StateOpen &&
             ssh_channel_poll(channel_, false) == SSH_EOF &&
             ssh_channel_poll(channel_, true)  == SSH_EOF ) {

            exit_code_ = ssh_channel_get_exit_status(channel_);

            LIBSSHQT_DEBUG("Process channel EOF");
            LIBSSHQT_DEBUG("Command exit code:"     << exit_code_);
            LIBSSHQT_DEBUG("Data in read buffer:"   << read_buffer_.size());
            LIBSSHQT_DEBUG("Data in write buffer:"  << write_buffer_.size());
            LIBSSHQT_DEBUG("Data in stderr buffer:" << stderr_buffer_.size());

            closeChannel();
            emit finished(exit_code_);
            return;
        }

        return;
    } break;

    } // End switch

    Q_ASSERT_X(false, __func__, "Case was not handled properly");
}

void LibsshQtProcess::checkIo()
{
    LibsshQtChannel::checkIo();
    if ( ! channel_ ) return;

    // Read stderr data from channel
    int read = 0;
    int read_size = ssh_channel_poll(channel_, true);
    if ( read_size > 0 ) {

        // Dont read more than buffer_size_ specifies.
        int max_read = buffer_size_ - stderr_buffer_.size();
        if ( read_size > max_read ) {
            read_size = max_read;
        }

        if ( read_size > 0 ) {
            char data[read_size];
            read = ssh_channel_read_nonblocking(channel_, data, read_size, true);


            stderr_buffer_.reserve(buffer_size_);
            stderr_buffer_.append(data, read_size);

            LIBSSHQT_DEBUG("stderr: Read:" << read <<
                           " Data in buffer:" << stderr_buffer_.size() <<
                           " Readable from channel:" <<
                           ssh_channel_poll(channel_, true));

            if ( read > 0 ) {
                emit stderr_->readyRead();
            }
        }
    }
}

void LibsshQtProcess::queueCheckIo()
{
    timer_.start();
}

void LibsshQtProcess::handleClientError()
{
    setState(LibsshQtProcess::StateClientError);
}

void LibsshQtProcess::handleStdoutOutput()
{
    if ( stdout_behaviour_ == OutputManual) return;

    while ( canReadLine()) {
        QString line = QString(readLine());
        handleOutput(stdout_behaviour_, stdout_output_prefix_, line);
    }
}

void LibsshQtProcess::handleStderrOutput()
{
    if ( stderr_behaviour_ == OutputManual) return;

    Q_ASSERT(stderr_);
    while ( stderr_->canReadLine()) {
        QString line = QString(stderr()->readLine());
        handleOutput(stderr_behaviour_, stderr_output_prefix_, line);
    }
}

void LibsshQtProcess::handleOutput(OutputBehaviour &behaviour,
                                   QString             &prefix,
                                   QString             &line)
{
    switch ( behaviour ) {
    case OutputManual:
    case OutputToDevNull:
        // Do nothing
        return;

    case OutputToQDebug:
        if ( line.endsWith('\n')) {
            line.remove(line.length() - 1, 1);
        }

        if ( prefix.isEmpty()) {
            qDebug() << qPrintable(line);
        } else {
            qDebug() << qPrintable(prefix)
                     << qPrintable(line);
        }
        return;
    }
}



//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// LibsshQtProcessStderr
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

LibsshQtProcessStderr::LibsshQtProcessStderr(LibsshQtProcess *parent) :
    QIODevice(parent),
    parent_(parent),
    err_buffer_(0)
{
}

qint64 LibsshQtProcessStderr::bytesAvailable() const
{
    return err_buffer_->size() + QIODevice::bytesAvailable();
}

qint64 LibsshQtProcessStderr::bytesToWrite() const
{
    return 0;
}

bool LibsshQtProcessStderr::isSequential()
{
    return true;
}

bool LibsshQtProcessStderr::canReadLine() const
{
    return QIODevice::canReadLine() ||
           err_buffer_->contains('\n') ||
           err_buffer_->size() >= parent_->buffer_size_ ||
           ( isOpen() == false && atEnd() == false );
}

bool LibsshQtProcessStderr::open(OpenMode mode)
{
    Q_ASSERT( mode == QIODevice::ReadOnly );
    Q_UNUSED( mode );

    // Set Unbuffered to disable QIODevice buffers.
    bool ret = QIODevice::open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    Q_ASSERT(ret);
    return ret;
}

void LibsshQtProcessStderr::close()
{
    QIODevice::close();
}

qint64 LibsshQtProcessStderr::readData(char *data, qint64 maxlen)
{
    Q_ASSERT(err_buffer_);
    parent_->queueCheckIo();

    qint64 copy_len  = maxlen;
    int    data_size = err_buffer_->size();

    if ( copy_len > data_size ) {
        copy_len = data_size;
    }

    memcpy(data, err_buffer_->constData(), copy_len);
    err_buffer_->remove(0, copy_len);
    return copy_len;
}

qint64 LibsshQtProcessStderr::writeData(const char */*data*/, qint64 /*len*/)
{
    return 0;
}




















