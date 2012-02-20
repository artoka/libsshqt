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

    Q_ENUMS(State)
    enum State
    {
        StateClosed,
        StateClosing,
        StateConnecting,
        StateIsKnown,
        StateUnknownHost,
        StateAuthChoose,
        StateAuthContinue,
        StateAuthNone,
        StateAuthAutoPubkey,
        StateAuthPassword,
        StateAuthNeedPassword,
        StateAuthKbi,
        StateAuthKbiQuestions,
        StateAuthFailed,
        StateOpened,
        StateError
    };

    Q_ENUMS(LogVerbosity)
    enum LogVerbosity
    {
        LogDisable                  = SSH_LOG_NOLOG,
        LogRare                     = SSH_LOG_RARE,
        LogProtocol                 = SSH_LOG_PROTOCOL,
        LogPacket                   = SSH_LOG_PACKET,
        LogFunction                 = SSH_LOG_FUNCTIONS
    };

    Q_ENUMS(HostState)
    enum HostState
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
        AuthMethodKbi               = SSH_AUTH_METHOD_INTERACTIVE
    };
    Q_DECLARE_FLAGS(AuthMethods, AuthMehodFlag)

    Q_FLAGS(UseAuthFlag)
    enum UseAuthFlag
    {
        UseAuthEmpty                = 0,    //<! Auth method not chosen
        UseAuthNone                 = 1<<0, //<! SSH None authentication method
        UseAuthAutoPubKey           = 1<<1, //<! Keys from ~/.ssh and ssh-agent
        UseAuthPassword             = 1<<2, //<! SSH Password auth method
        UseAuthKbi                  = 1<<3  //<! SSH KBI auth method
    };
    Q_DECLARE_FLAGS(UseAuths, UseAuthFlag)

    class KbiQuestion
    {
    public:
        QString instruction;
        QString question;
        bool    showAnswer;
    };


    LibsshQtClient(QObject *parent = 0);
    ~LibsshQtClient();

    static const char *enumToString(const LogVerbosity  value);
    static const char *enumToString(const State         value);
    static const char *enumToString(const HostState     value);
    static const char *enumToString(const AuthMehodFlag value);
    static const char *enumToString(const UseAuthFlag   value);

    static QString flagsToString(const AuthMethods flags);
    static QString flagsToString(const UseAuths    flags);


    // Options
    void setDebug(bool enabled);
    void setUsername(QString username);
    void setHostname(QString hostname);
    void setPort(quint16 port);
    void setVerbosity(LogVerbosity loglevel);
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
    void useAuth(UseAuths auths, bool enabled);
    void useAuthDefaults();
    void useNoneAuth(bool enabled);
    void useAutoKeyAuth(bool enabled);
    void usePasswordAuth(bool enabled);
    void useKbiAuth(bool enabled);

    void setPassword(QString password);
    QList<KbiQuestion> kbiQuestions();
    void setKbiAnswers(QStringList answers);

    AuthMethods supportedAuthMethods();
    UseAuths enabledAuths();
    UseAuths failedAuths();

    // Doing something
    LibsshQtProcess *runCommand(QString command);

    // Functions for handling unknown hosts
    HostState unknownHostType();
    QString unknownHostMessage();
    QString hostPublicKeyHash();
    bool markCurrentHostKnown();

    // Errors handling
    QString errorCodeAndMessage() const;
    QString errorMessage() const;
    int errorCode() const;

    State state() const;
    ssh_session sshSession();
    void enableWritableNotifier();

signals:
    void unknownHost();
    void chooseAuth();
    void authFailed();
    void needPassword();    //!< Use setPassword() to set password
    void needKbiAnswers();  //!< Use setKbiAnswers() set answers
    void opened();
    void closed();
    void error();

    // Signals for LIBSSHQT child objects:
    void doProcessState();  //!< All children must process their state
    void doCleanup();       //!< All children must release libssh resources

private:
    void setState(State state);
    void tryNextAuth();
    void setUpNotifiers();
    void processState();
    void handleAuthResponse(int rc, const char *func, UseAuthFlag auth);

private slots:
    void handleSocketReadable(int socket);
    void handleSocketWritable(int socket);
    void processStateGuard();

private:
    QString         debug_prefix_;
    bool            debug_output_;

    QTimer          timer_;
    ssh_session     session_;
    State           state_;
    bool            process_state_running_;
    bool            enable_writable_nofifier_;

    quint16         port_;
    QString         hostname_;
    QString         username_;

    QSocketNotifier *read_notifier_;
    QSocketNotifier *write_notifier_;

    HostState       unknown_host_type_;
    QString         unknwon_host_key_hex_;

    UseAuths        use_auths_;
    UseAuths        failed_auths_;
    UseAuthFlag     succeeded_auth_;

    bool            password_set_;
    QString         password_;
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

    static const char *enumToString(const EofState flag);

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
    virtual void queueCheckIo() = 0;
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

    LibsshQtProcess(LibsshQtClient *parent);
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
    LibsshQtProcess *parent_;
    QByteArray *err_buffer_;
};


// Include <QDebug> before "libsshqt.h" if you want to use these operators
#ifdef QDEBUG_H

inline QDebug operator<<(QDebug dbg, const LibsshQtClient *client)
{
    if ( client->state() == LibsshQtClient::StateError ) {
        dbg.nospace() << "LibsshQtClient( "
                      << LibsshQtClient::enumToString(client->state()) << ", "
                      << client->errorCode() << ", "
                      << client->errorMessage() << " )";
    } else {
        dbg.nospace() << "LibsshQtClient( "
                      << LibsshQtClient::enumToString(client->state())
                      << " )";
    }
    return dbg.space();
}

inline QDebug operator<<(QDebug dbg, const LibsshQtClient::State value)
{
    dbg << LibsshQtClient::enumToString(value);
    return dbg;
}

inline QDebug operator<<(QDebug dbg, const LibsshQtClient::LogVerbosity value)
{
    dbg << LibsshQtClient::enumToString(value);
    return dbg;
}

inline QDebug operator<<(QDebug dbg, const LibsshQtClient::HostState value)
{
    dbg << LibsshQtClient::enumToString(value);
    return dbg;
}

inline QDebug operator<<(QDebug dbg, const LibsshQtClient::AuthMehodFlag flag)
{
    dbg << LibsshQtClient::enumToString(flag);
    return dbg;
}

inline QDebug operator<<(QDebug dbg, const LibsshQtClient::UseAuthFlag flag)
{
    dbg << LibsshQtClient::enumToString(flag);
    return dbg;
}

inline QDebug operator<<(QDebug dbg, const LibsshQtClient::AuthMethods flags)
{
    dbg << qPrintable(LibsshQtClient::flagsToString(flags));
    return dbg;
}

inline QDebug operator<<(QDebug dbg, const LibsshQtClient::UseAuths flags)
{
    dbg << qPrintable(LibsshQtClient::flagsToString(flags));
    return dbg;
}

inline QDebug operator<<(QDebug dbg, const LibsshQtClient::KbiQuestion &question)
{
    dbg.nospace() << "KbiQuestion( "
                  << question.instruction << ", "
                  << question.question << ", "
                  << question.showAnswer << " )";
    return dbg.space();
}


inline QDebug operator<<(QDebug dbg, const LibsshQtChannel::EofState flag)
{
    dbg << LibsshQtChannel::enumToString(flag);
    return dbg;
}


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

#endif // LIBSSHQT_H


























