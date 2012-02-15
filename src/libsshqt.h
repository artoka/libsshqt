#ifndef LIBSSHQT_H
#define LIBSSHQT_H

#include <QObject>
#include <QTimer>
#include <QIODevice>
#include <QSocketNotifier>
#include <libssh/libssh.h>

/*!

    LIBSSHQT - The LibSsh Qt Wrapper

*/

class QUrl;
class LibsshQtClient;
class LibsshQtChannel;
class LibsshQtProcess;
class LibsshQtProcessStderr;


/*!

    LibsshQtClient

*/
class LibsshQtClient : public QObject
{
    Q_OBJECT

public:

    Q_FLAGS(StateFlag)
    enum StateFlag
    {
        StateClosed,
        StateClosing,
        StateConnecting,
        StateIsKnown,
        StateUnknownHost,
        StateChooseAuth,
        StateAutoAuth,
        StatePasswordAuth,
        StateAuthFailed,
        StateOpened,
        StateError
    };

    Q_FLAGS(LogFlag)
    enum LogFlag
    {
        LogDisable                  = SSH_LOG_NOLOG,
        LogRare                     = SSH_LOG_RARE,
        LogProtocol                 = SSH_LOG_PROTOCOL,
        LogPacket                   = SSH_LOG_PACKET,
        LogFunction                 = SSH_LOG_FUNCTIONS
    };

    Q_FLAGS(HostFlag)
    enum HostFlag
    {
        HostKnown                   = SSH_SERVER_KNOWN_OK,
        HostUnknown                 = SSH_SERVER_NOT_KNOWN,
        HostKeyChanged              = SSH_SERVER_KNOWN_CHANGED,
        HostKeyTypeChanged          = SSH_SERVER_FOUND_OTHER,
        HostKnownHostsFileMissing   = SSH_SERVER_FILE_NOT_FOUND
    };

    Q_FLAGS(AuthMehodFlag)
    enum AuthMehodFlag
    {
        AuthMethodUnknown           = SSH_AUTH_METHOD_UNKNOWN,
        AuthMethodNone              = SSH_AUTH_METHOD_NONE,
        AuthMethodPassword          = SSH_AUTH_METHOD_PASSWORD,
        AuthMethodPublicKey         = SSH_AUTH_METHOD_PUBLICKEY,
        AuthMethodHostBased         = SSH_AUTH_METHOD_HOSTBASED,
        AuthMethodInteractive       = SSH_AUTH_METHOD_INTERACTIVE
    };
    Q_DECLARE_FLAGS(AuthMethods, AuthMehodFlag)


    LibsshQtClient(QObject *parent = 0);
    ~LibsshQtClient();

    static const char *flagToString(const LogFlag       flag);
    static const char *flagToString(const StateFlag     flag);
    static const char *flagToString(const HostFlag      flag);
    static const char *flagToString(const AuthMehodFlag flag);

    static QString flagsToString(const AuthMethods flags);


    // Options
    void setDebug(bool enabled);
    void setUsername(QString username);
    void setHostname(QString hostname);
    void setPort(quint16 port);
    void setVerbosity(LogFlag loglevel);
    void setUrl(QUrl &url);

    bool isDebugEnabled() const;
    QString username() const;
    QString hostname() const;
    quint16 port() const;
    QUrl url() const;

    // Connection
    bool isOpen();
public slots:
    void connectToHost();
    void connectToHost(const QString hostName);
    void connectToHost(const QString hostName, unsigned int port);
    void disconnectFromHost();
public:

    // Authentication
    AuthMethods supportedAuthMethods();
    void useAutoKeyAuth(bool enabled);
    void usePasswordAuth(bool enabled, QString password);

    // Doing something
    LibsshQtProcess *runCommand(QString command);

    // Functions for handling unknown hosts
    HostFlag unknownHostType();
    QString unknownHostMessage();
    QString hostPublicKeyHash();
    bool markCurrentHostKnown();

    // Errors handling
    QString errorCodeAndMessage() const;
    QString errorMessage() const;
    int errorCode() const;

    StateFlag state() const;
    ssh_session sshSession();

signals:
    void unknownHost();
    void chooseAuth();
    void authFailed();
    void opened();
    void closed();
    void error();

    // Signals for LIBSSHQT child objects:
    void doProcessState();  //!< All children must process their state
    void doCleanup();       //!< All children must release libssh resources

private:
    void setState(StateFlag state);
    void setUpNotifiers();

private slots:
    void checkSocket(int socket);
    void processState();

private:
    QString     debug_prefix_;
    bool        debug_output_;

    QTimer      timer_;
    ssh_session session_;
    StateFlag   state_;

    quint16     port_;
    QString     hostname_;
    QString     username_;

    QSocketNotifier *read_notifier_;
    QSocketNotifier *write_notifier_;

    HostFlag    unknown_host_type_;
    QString     unknwon_host_key_hex_;

    int         auth_attemp_;
    bool        use_autokey_;
    bool        use_password_;

    QString     password_;
};


/*!

    LibsshQtChannel

*/
class LibsshQtChannel : public QIODevice
{
    Q_OBJECT

public:

    Q_FLAGS(EofState)
    enum EofState
    {
        EofNotSent,
        EofQueued,
        EofSent
    };

    LibsshQtChannel(LibsshQtClient *parent);
    ~LibsshQtChannel();
    LibsshQtClient *client();

    static const char *flagToString(const EofState flag);

    void setWriteSize(int write_size);
    void setReadBufferSize(int buffer_size);
    int getWriteSize();
    int getBufferSize();

    void sendEof();
    EofState eofState();

    // Error handling
    QString errorCodeAndMessage() const;
    QString errorMessage() const;
    int errorCode() const;

public slots:
    virtual void closeChannel() = 0;
    virtual void openChannel() = 0;

    // Implement QIODevice API
public:
    qint64 bytesAvailable() const;
    qint64 bytesToWrite() const;
    bool isSequential();
    bool canReadLine() const;

protected:
    bool open(OpenMode mode = QIODevice::ReadWrite);
    void close();
    qint64 readData(char *data, qint64 maxlen);
    qint64 writeData(const char *data, qint64 len);

public slots:
    virtual void checkIo();
    virtual void processState() = 0;

protected:
    LibsshQtClient *client_;
    QString     debug_prefix_;
    bool        debug_output_;

    ssh_channel channel_;
    EofState    eof_state_;

    int         buffer_size_;
    int         write_size_;
    QByteArray  read_buffer_;
    QByteArray  write_buffer_;
};


/*!

    LibsshQtProcess

    By default LibsshQtProcess will send all process output to QDebug(), to
    change this behaviour see setStderrBehaviour() and setStdoutBehaviour().

*/
class LibsshQtProcess : public LibsshQtChannel
{
    Q_OBJECT

public:
    Q_FLAGS(StateFlag)
    enum StateFlag {
        StateClosed,
        StateClosing,
        StateWaitClient,
        StateOpening,
        StateExec,
        StateOpen,
        StateError,
        StateClientError
    };

    Q_FLAGS(OutputBehaviourFlag)
    enum OutputBehaviourFlag
    {
        OutputManual,   //!< No automatic handling, read output manually
        OutputToQDebug, //!< All output is sent to QDebug()
        OutputToDevNull //!< All output is completely ignored
    };

    LibsshQtProcess(LibsshQtClient *parent);
    ~LibsshQtProcess();

    static const char *flagToString(const StateFlag flag);
    static const char *flagToString(const OutputBehaviourFlag flag);

    void setCommand(QString command);
    int exitCode() const;

    void setStdoutBehaviour(OutputBehaviourFlag behaviour, QString prefix = "");
    void setStderrBehaviour(OutputBehaviourFlag behaviour, QString prefix = "");
    LibsshQtProcessStderr *stderr();

    void openChannel();
    void closeChannel();

    StateFlag state() const;

signals:
    void opened();
    void closed();
    void error();
    void finished(int exit_code);

protected:
    void setState(StateFlag state);
    void processState();
    void checkIo();

private slots:
    void handleClientError();
    void handleStdoutOutput();
    void handleStderrOutput();
    void handleOutput(OutputBehaviourFlag &behaviour,
                      QString &prefix, QString &line);

private:
    QTimer      timer_;
    StateFlag   state_;
    QString     command_;
    int         exit_code_;

    QByteArray         stderr_buffer_;
    LibsshQtProcessStderr *stderr_;

    OutputBehaviourFlag stdout_behaviour_;
    QString             stdout_output_prefix_;

    OutputBehaviourFlag stderr_behaviour_;
    QString             stderr_output_prefix_;
};


/*!

    LibsshQtChannelStderr

*/
class LibsshQtProcessStderr : public QIODevice
{
    Q_OBJECT
    friend class LibsshQtProcess;

public:
    LibsshQtProcessStderr(LibsshQtProcess *parent);

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
    QByteArray *err_buffer_;
};


// Include <QDebug> before "libsshqt.h" if you want to use these operators
#ifdef QDEBUG_H

inline QDebug operator<<(QDebug dbg, const LibsshQtClient *client)
{
    if ( client->state() == LibsshQtClient::StateError ) {
        dbg.nospace() << "LibsshQtClient("
                      << LibsshQtClient::flagToString(client->state()) << ", "
                      << client->errorCode() << ", "
                      << client->errorMessage() << ")";
    } else {
        dbg.nospace() << "LibsshQtClient("
                      << LibsshQtClient::flagToString(client->state())
                      << ")";
    }
    return dbg.space();
}

inline QDebug operator<<(QDebug dbg, const LibsshQtClient::StateFlag flag)
{
    dbg << LibsshQtClient::flagToString(flag);
    return dbg;
}

inline QDebug operator<<(QDebug dbg, const LibsshQtClient::LogFlag flag)
{
    dbg << LibsshQtClient::flagToString(flag);
    return dbg;
}

inline QDebug operator<<(QDebug dbg, const LibsshQtClient::HostFlag flag)
{
    dbg << LibsshQtClient::flagToString(flag);
    return dbg;
}

inline QDebug operator<<(QDebug dbg, const LibsshQtClient::AuthMethods flags)
{
    dbg << qPrintable(LibsshQtClient::flagsToString(flags));
    return dbg;
}

inline QDebug operator<<(QDebug dbg, const LibsshQtClient::AuthMehodFlag flag)
{
    dbg << LibsshQtClient::flagToString(flag);
    return dbg;
}


inline QDebug operator<<(QDebug dbg, const LibsshQtChannel::EofState flag)
{
    dbg << LibsshQtChannel::flagToString(flag);
    return dbg;
}


inline QDebug operator<<(QDebug dbg, const LibsshQtProcess *process)
{
    if ( process->state() == LibsshQtProcess::StateError ) {
        dbg.nospace() << "LibsshQtProcess("
                      << LibsshQtProcess::flagToString(process->state()) << ", "
                      << process->errorCode() << ", "
                      << process->errorMessage() << ")";
    } else {
        dbg.nospace() << "LibsshQtProcess("
                      << LibsshQtProcess::flagToString(process->state())
                      << ")";
    }
    return dbg.space();
}

inline QDebug operator<<(QDebug dbg, const LibsshQtProcess::StateFlag flag)
{
    dbg << LibsshQtProcess::flagToString(flag);
    return dbg;
}

inline QDebug operator<<(QDebug dbg, const LibsshQtProcess::OutputBehaviourFlag flag)
{
    dbg << LibsshQtProcess::flagToString(flag);
    return dbg;
}

#endif

#endif // LIBSSHQT_H


























