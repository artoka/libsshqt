#ifndef LIBSSHQTPROCESS_H
#define LIBSSHQTPROCESS_H

#include <QObject>
#include <QTimer>
#include <QIODevice>
#include <QSocketNotifier>
#include "libsshqtchannel.h"

class LibsshQtProcessStderr;

/*!

    LibsshQtProcess

    By default LibsshQtProcess will send all process output to QDebug(), to
    change this behaviour see setStderrBehaviour() and setStdoutBehaviour().

*/
class LibsshQtProcess : public LibsshQtChannel
{
    Q_OBJECT
    friend class LibsshQtProcessStderr;

public:
    Q_ENUMS(State)
    enum State {
        StateClosed,
        StateClosing,
        StateWaitClient,
        StateOpening,
        StateExec,
        StateOpen,
        StateError,
        StateClientError
    };

    Q_ENUMS(OutputBehaviour)
    enum OutputBehaviour
    {
        OutputManual,   //!< No automatic handling, read output manually
        OutputToQDebug, //!< All output is sent to QDebug()
        OutputToDevNull //!< All output is completely ignored
    };

    explicit LibsshQtProcess(LibsshQtClient *parent);
    ~LibsshQtProcess();

    static const char *enumToString(const State value);
    static const char *enumToString(const OutputBehaviour value);

    void setCommand(QString command);
    QString command();
    int exitCode() const;

    void setStdoutBehaviour(OutputBehaviour behaviour, QString prefix = "");
    void setStderrBehaviour(OutputBehaviour behaviour, QString prefix = "");
    LibsshQtProcessStderr *stderr();

    void openChannel();
    void closeChannel();

    State state() const;

signals:
    void opened();
    void closed();
    void error();
    void finished(int exit_code);

protected:
    void setState(State state);
    void processState();
    void checkIo();
    void queueCheckIo();

private slots:
    void handleClientError();
    void handleStdoutOutput();
    void handleStderrOutput();
    void handleOutput(OutputBehaviour &behaviour,
                      QString &prefix, QString &line);

private:
    QTimer      timer_;
    State       state_;
    QString     command_;
    int         exit_code_;

    QByteArray              stderr_buffer_;
    LibsshQtProcessStderr  *stderr_;

    OutputBehaviour         stdout_behaviour_;
    QString                 stdout_output_prefix_;

    OutputBehaviour         stderr_behaviour_;
    QString                 stderr_output_prefix_;
};


/*!

    LibsshQtChannelStderr

*/
class LibsshQtProcessStderr : public QIODevice
{
    Q_OBJECT
    friend class LibsshQtProcess;

public:
    explicit LibsshQtProcessStderr(LibsshQtProcess *parent);

    qint64 bytesAvailable() const;
    qint64 bytesToWrite() const;
    bool isSequential();
    bool canReadLine() const;

protected:
    bool open(OpenMode mode = QIODevice::ReadOnly);
    void close();
    qint64 readData(char *data, qint64 maxlen);
    qint64 writeData(const char *data, qint64 len);

private:
    LibsshQtProcess *parent_;
    QByteArray *err_buffer_;
};


// Include <QDebug> before "libsshqt.h" if you want to use these operators
#ifdef QDEBUG_H

inline QDebug operator<<(QDebug dbg, const LibsshQtProcess *process)
{
    if ( process->state() == LibsshQtProcess::StateError ) {
        dbg.nospace() << "LibsshQtProcess( "
                      << LibsshQtProcess::enumToString(process->state()) << ", "
                      << process->errorCode() << ", "
                      << process->errorMessage() << " )";
    } else {
        dbg.nospace() << "LibsshQtProcess( "
                      << LibsshQtProcess::enumToString(process->state())
                      << " )";
    }
    return dbg.space();
}

inline QDebug operator<<(QDebug dbg, const LibsshQtProcess::State value)
{
    dbg << LibsshQtProcess::enumToString(value);
    return dbg;
}

inline QDebug operator<<(QDebug dbg, const LibsshQtProcess::OutputBehaviour value)
{
    dbg << LibsshQtProcess::enumToString(value);
    return dbg;
}

#endif

#endif // LIBSSHQTPROCESS_H


























