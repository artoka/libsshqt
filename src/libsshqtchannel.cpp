
#include <QDebug>
#include <QMetaEnum>
#include <QProcessEnvironment>
#include <QCoreApplication>
#include <QUrl>

#include "libsshqtchannel.h"
#include "libsshqtclient.h"
#include "libsshqtdebug.h"

const char *LibsshQtChannel::enumToString(const EofState flag)
{
    return staticMetaObject.enumerator(
                staticMetaObject.indexOfEnumerator("EofState"))
                    .valueToKey(flag);
}

LibsshQtChannel::LibsshQtChannel(bool            is_stderr,
                                 LibsshQtClient *client,
                                 QObject        *parent) :
    QIODevice(parent),
    debug_prefix_(LibsshQt::debugPrefix(this)),
    debug_output_(client->isDebugEnabled()),
    client_(client),
    channel_(0),
    is_stderr_(is_stderr),
    eof_state_(EofNotSent),
    buffer_size_(1024 * 16),
    write_size_(1024 * 16)
{
    connect(client_, SIGNAL(debugChanged()),
            this,    SLOT(handleDebugChanged()));
}

/*!
    Is the channel at end?

    @par Channel is at end if
    - IODevice is closed
    - Read buffer is empty and more data won't be forthcoming
*/
bool LibsshQtChannel::atEnd() const
{
    // Channel is NULL
    // IODevice is closed
    // Buffer is empty AND ( Channel closed OR Channel is EOF )
    return channel_ == 0 ||
            isOpen() == false ||
            ( read_buffer_.isEmpty() &&
              ( ssh_channel_is_open(channel_) == false ||
                ssh_channel_poll(channel_, is_stderr_) == SSH_EOF ));
}

qint64 LibsshQtChannel::bytesAvailable() const
{
    return read_buffer_.size() + QIODevice::bytesAvailable();
}

qint64 LibsshQtChannel::bytesToWrite() const
{
    return write_buffer_.size();
}

bool LibsshQtChannel::isSequential()
{
    return true;
}

/*!
    Is a line available for reading?

    @par A line available for reading if:
    - Newline is found in read buffer
    - Read buffer is full
    - Data is available in read buffer and more data won't be forthcoming
*/
bool LibsshQtChannel::canReadLine() const
{
    // Newline in buffer
    // Buffer is full
    // Data in buffer and (closed or Channel NULL or EOF )
    return QIODevice::canReadLine() ||
           read_buffer_.contains('\n') ||
           read_buffer_.size() >= buffer_size_ ||
            ( read_buffer_.isEmpty() == false &&
              ( isOpen() == false ||
                channel_ == 0 ||
                ( ssh_channel_is_open(channel_) &&
                  ssh_channel_poll(channel_, is_stderr_) == SSH_EOF )));
}

qint64 LibsshQtChannel::readData(char *data, qint64 maxlen)
{
    queueCheckIo();

    qint64 copy_len  = maxlen;
    int    data_size = read_buffer_.size();

    if ( copy_len > data_size ) {
        copy_len = data_size;
    }

    memcpy(data, read_buffer_.constData(), copy_len);
    read_buffer_.remove(0, copy_len);
    return copy_len;
}

qint64 LibsshQtChannel::writeData(const char *data, qint64 len)
{
    if ( openMode() == ReadOnly ) {
        LIBSSHQT_CRITICAL("Cannot write to channel because" <<
                          "ReadOnly flag is set");
        return 0;

    } else if ( eof_state_ != EofNotSent ) {
        LIBSSHQT_CRITICAL("Cannot write to channel because EOF state is" <<
                          eof_state_);
        return 0;

    } else {
        client_->enableWritableNotifier();
        write_buffer_.reserve(write_size_);
        write_buffer_.append(data, len);
        return len;
    }
}

void LibsshQtChannel::checkIo()
{
    if ( ! channel_ ) return;

    bool emit_ready_read = false;
    bool emit_bytes_written = false;

    int read_size = 0;
    int read_available = ssh_channel_poll(channel_, is_stderr_);
    if ( read_available > 0 ) {

        // Dont read more than buffer_size_ specifies.
        int max_read = buffer_size_ - read_buffer_.size();
        if ( read_available > max_read ) {
            read_available = max_read;
        }

        if ( read_available > 0 ) {
            char data[read_available + 1];
            data[read_available] = 0;

            read_size = ssh_channel_read_nonblocking(channel_,
                                                     data,
                                                     read_available,
                                                     is_stderr_);
            Q_ASSERT(read_size >= 0);

            read_buffer_.reserve(buffer_size_);
            read_buffer_.append(data, read_size);

            LIBSSHQT_DEBUG("Read:" << read_size <<
                           " Data in buffer:" << read_buffer_.size() <<
                           " Readable from channel:" <<
                           ssh_channel_poll(channel_, is_stderr_));
            if ( read_size > 0 ) {
                emit_ready_read = true;
            }
        }
    }

    int written = 0;
    int writable = 0;
    if ( openMode() != ReadOnly ) {

        writable = write_buffer_.size();
        if ( writable > write_size_ ) {
            writable = write_size_;
        }

        if ( writable > 0 ) {
            written = ssh_channel_write(channel_,
                                        write_buffer_.constData(),
                                        writable);
            Q_ASSERT(written >= 0);
            write_buffer_.remove(0, written);

            LIBSSHQT_DEBUG("Wrote" << written << "bytes to channel")
                    if ( written > 0 ) {
                emit_bytes_written = true;
            }
        }

        // Write more data once the socket is ready
        if ( write_buffer_.size() > 0 ) {
            client_->enableWritableNotifier();
        }
    }

    // Send EOF once all data has been written to channel
    if ( eof_state_ == EofQueued && write_buffer_.size() == 0 ) {
        LIBSSHQT_DEBUG("Sending EOF to channel");
        ssh_channel_send_eof(channel_);
        eof_state_ = EofSent;
    }

    // Emit signals here, so that somebody wont call closeChannel() while
    // when we are reading from it.
    if ( emit_ready_read ) {
        emit readyRead();
    }
    if ( emit_bytes_written ) {
        emit bytesWritten(written);
    }
}

/*!
    Get parent LibsshQtClient object.
*/
LibsshQtClient *LibsshQtChannel::client()
{
    return client_;
}

/*!
    Set maximum amount of data that LibsshQtChannel will write to the channel
    in one go.
*/
void LibsshQtChannel::setWriteSize(int write_size)
{
    static const int min_size = 4096;
    Q_ASSERT( write_size >= min_size );

    if ( write_size_ >= min_size ) {
        write_size_ = write_size;
    } else {
        write_size_ = min_size;
    }
}

/*!
    Set read buffer size.
*/
void LibsshQtChannel::setReadBufferSize(int buffer_size)
{
    static const int min_size = 4096;
    Q_ASSERT( buffer_size >= min_size );

    if ( buffer_size >= min_size ) {
        buffer_size_ = buffer_size;
    } else {
        buffer_size_ = min_size;
    }
}

int LibsshQtChannel::getBufferSize()
{
    return buffer_size_;
}

int LibsshQtChannel::getWriteSize()
{
    return write_size_;
}

/*!
    Send EOF to the channel once write buffer has been written to the channel.
 */
void LibsshQtChannel::sendEof()
{
    if ( eof_state_ == EofNotSent ) {
        LIBSSHQT_DEBUG("EOF queued");
        eof_state_ = EofQueued;
    }
}

LibsshQtChannel::EofState LibsshQtChannel::eofState()
{
    return eof_state_;
}

/*!
    Did the error happen in LibsshQtClient instead of LibsshQtChannel?

    LibsshQtChannel inherists errors from LibsshQtClient, so that if client
    fails because of an error, then LibsshQtChannel will also fail.

    You can use this function to check if the error actually happend in
    LibsshQtClient instead of LibsshQtChannel.
*/
bool LibsshQtChannel::isClientError() const
{
    return client_->state() == LibsshQtClient::StateError;
}

/*!
    Get error code and message from libssh.
*/
QString LibsshQtChannel::errorCodeAndMessage() const
{
    return QString("%1 (%2 %3)")
            .arg(errorMessage())
            .arg(tr("libssh error code"))
            .arg(errorCode());
}

/*!
    Get error message from libssh.
*/
QString LibsshQtChannel::errorMessage() const
{
    if ( client_->state() == LibsshQtClient::StateError ) {
        return client_->errorMessage();
    } else if ( channel_ ) {
        return QString(ssh_get_error(channel_));
    } else {
        return QString();
    }
}

/*!
    Get error code from libssh.
*/
int LibsshQtChannel::errorCode() const
{
    if ( client_->state() == LibsshQtClient::StateError ) {
        return client_->errorCode();
    } else if ( channel_ ) {
        return ssh_get_error_code(channel_);
    } else {
        return 0;
    }
}

void LibsshQtChannel::handleDebugChanged()
{
    debug_output_ = client_->isDebugEnabled();
}






















