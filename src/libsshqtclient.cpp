
#include <QDebug>
#include <QMetaEnum>
#include <QProcessEnvironment>
#include <QCoreApplication>
#include <QUrl>

#include "libsshqtclient.h"
#include "libsshqtprocess.h"
#include "libsshqtdebug.h"


LibsshQtClient::LibsshQtClient(QObject *parent) :
    QObject(parent),
    debug_output_(false),
    session_(0),
    state_(StateClosed),
    process_state_running_(false),
    enable_writable_nofifier_(false),
    port_(22),
    read_notifier_(0),
    write_notifier_(0),
    unknown_host_type_(HostKnown),
    password_set_(false)
{
    debug_prefix_ = LibsshQt::debugPrefix(this);

    if ( QProcessEnvironment::systemEnvironment().contains("LIBSSHQT_DEBUG")) {
        debug_output_ = true;
        LIBSSHQT_DEBUG("Constructor");
    }

    timer_.setSingleShot(true);
    timer_.setInterval(0);
    connect(&timer_, SIGNAL(timeout()), this, SLOT(processStateGuard()));

    if (debug_output_) {
        setVerbosity(LogProtocol);
    } else {
        setVerbosity(LogDisable);
    }

    // Ensure that connections are always properly closed
    connect(qApp, SIGNAL(aboutToQuit()), this, SLOT(disconnectFromHost()));
}

LibsshQtClient::~LibsshQtClient()
{
    LIBSSHQT_DEBUG("Destructor");

    disconnectFromHost();
    if ( session_ ) {
        ssh_free(session_);
        session_ = 0;
    }
}

const char *LibsshQtClient::enumToString(const LogVerbosity value)
{
    return staticMetaObject.enumerator(
                staticMetaObject.indexOfEnumerator("LogVerbosity"))
                    .valueToKey(value);
}

const char *LibsshQtClient::enumToString(const State value)
{
    return staticMetaObject.enumerator(
                staticMetaObject.indexOfEnumerator("State"))
                    .valueToKey(value);
}

const char *LibsshQtClient::enumToString(const HostState value)
{
    return staticMetaObject.enumerator(
                staticMetaObject.indexOfEnumerator("HostState"))
                    .valueToKey(value);
}

const char *LibsshQtClient::enumToString(const AuthMehodFlag value)
{
    return staticMetaObject.enumerator(
                staticMetaObject.indexOfEnumerator("AuthMehodFlag"))
                    .valueToKey(value);
}

const char *LibsshQtClient::enumToString(const UseAuthFlag value)
{
    return staticMetaObject.enumerator(
                staticMetaObject.indexOfEnumerator("UseAuthFlag"))
                    .valueToKey(value);
}

QString LibsshQtClient::flagsToString(const AuthMethods flags)
{
    QStringList list;

    int val = flags;
    if ( val == AuthMethodUnknown ) {
        list << enumToString(AuthMethodUnknown);

    } else {

        if ( flags.testFlag(AuthMethodNone)) {
            list << enumToString(AuthMethodNone);
        }

        if ( flags.testFlag(AuthMethodPassword)) {
            list << enumToString(AuthMethodPassword);
        }

        if ( flags.testFlag(AuthMethodPublicKey)) {
            list << enumToString(AuthMethodPublicKey);
        }

        if ( flags.testFlag(AuthMethodHostBased)) {
            list << enumToString(AuthMethodHostBased);

        }

        if ( flags.testFlag(AuthMethodKbi)) {
            list << enumToString(AuthMethodKbi);
        }
    }

    return QString("AuthMethods( %1 )").arg(list.join(", "));
}

QString LibsshQtClient::flagsToString(const UseAuths flags)
{
    QStringList list;

    if ( flags == UseAuthEmpty ) {
        list << enumToString(UseAuthEmpty);

    } else {

        if ( flags.testFlag(UseAuthNone)) {
            list << enumToString(UseAuthNone);
        }

        if ( flags.testFlag(UseAuthAutoPubKey)) {
            list << enumToString(UseAuthAutoPubKey);
        }

        if ( flags.testFlag(UseAuthPassword)) {
            list << enumToString(UseAuthPassword);
        }

        if ( flags.testFlag(UseAuthKbi)) {
            list << enumToString(UseAuthKbi);

        }
    }

    return QString("UseAuths( %1 )").arg(list.join(", "));
}

/*!
    Enable or disable debug messages.

    If logging is enabled libssh log level is set to LogProtocol and
    LIBSSHQT debug messages are enabled.
*/
void LibsshQtClient::setDebug(bool enabled)
{
    if ( enabled ) {
        debug_output_ = true;
        LIBSSHQT_DEBUG("Enabling debug messages");
        setVerbosity(LogProtocol);

    } else {
        LIBSSHQT_DEBUG("Disabling debug messages");
        debug_output_ = false;
        setVerbosity(LogDisable);
    }

    emit debugChanged();
}

void LibsshQtClient::setUsername(QString username)
{
    Q_ASSERT( state_ == StateClosed );

    if ( state_ == StateClosed ) {
        username_ = username;
    } else {
        LIBSSHQT_CRITICAL("Cannot set AAAAAAA when state is" << state_);
    }
}

void LibsshQtClient::setHostname(QString hostname)
{
    Q_ASSERT( state_ == StateClosed );

    if ( state_ == StateClosed ) {
        hostname_ = hostname;
    } else {
        LIBSSHQT_CRITICAL("Cannot set AAAAAAA when state is" << state_);
    }
}

void LibsshQtClient::setPort(quint16 port)
{
    Q_ASSERT( state_ == StateClosed );

    if ( state_ == StateClosed ) {
        port_ = port;
    } else {
        LIBSSHQT_CRITICAL("Cannot set AAAAAAA when state is" << state_);
    }
}

/*!
    Set libssh logging level.
*/
void LibsshQtClient::setVerbosity(LogVerbosity loglevel)
{
    log_verbosity_ = loglevel;
    if ( session_ ) {
        int tmp_verbosity = log_verbosity_;
        setLibsshOption(SSH_OPTIONS_LOG_VERBOSITY, "SSH_OPTIONS_LOG_VERBOSITY",
                        &tmp_verbosity, enumToString(log_verbosity_));
    }
}

bool LibsshQtClient::isDebugEnabled() const
{
    return debug_output_;
}

QString LibsshQtClient::username() const
{
    return username_;
}

QString LibsshQtClient::hostname() const
{
    return hostname_;
}

quint16 LibsshQtClient::port() const
{
    return port_;
}

QUrl LibsshQtClient::url() const
{
    QUrl url;
    url.setHost(hostname_);
    url.setPort(port_);
    url.setUserName(username_);
    url.setScheme("ssh");
    return url;
}

/*!
    Set hostname, port and username options from QUrl
*/
void LibsshQtClient::setUrl(QUrl &url)
{
    if ( url.scheme().toLower() == "ssh" ) {
        LIBSSHQT_DEBUG("Setting options from URL:" << url);

        if ( url.port() > -1 ) {
            setPort(url.port());
        } else {
            setPort(22);
        }

        setHostname(url.host());
        setUsername(url.userName());

    } else {
        LIBSSHQT_CRITICAL("Not SSH URL:" << url);
    }
}

/*!
    Returns true if connection is successfully connected and authenticated.
*/
bool LibsshQtClient::isOpen()
{
    return state_ == StateOpened;
}

/*!
    Open connection to the host.
*/
void LibsshQtClient::connectToHost()
{
    if ( state_ == StateClosed ) {
        setState(StateInit);
        timer_.start();
    }
}

/*!
    Set hostname and open connection to the host.
*/
void LibsshQtClient::connectToHost(const QString hostName)
{
    if ( state_ == StateClosed ) {
        setHostname(hostName);
        setPort(22);
        connectToHost();
    }
}

/*!
    Set hostname and port and open connection to the host.
*/
void LibsshQtClient::connectToHost(const QString hostName, unsigned int port)
{
    if ( state_ == StateClosed ) {
        setHostname(hostName);
        setPort(port);
        connectToHost();
    }
}

/*!
    Close connection to host.
*/
void LibsshQtClient::disconnectFromHost()
{
    if ( state_ != StateClosed &&
         state_ != StateClosing ) {

        // Prevent recursion
        setState(StateClosing);

        // Child libsshqt objects must handle this and release all libssh
        // resources
        emit doCleanup();

        destroyNotifiers();

        ssh_disconnect(session_);
        ssh_free(session_);
        session_ = 0;

        setState(StateClosed);
    }
}

/*!
    Enable or disable one or more authentications.
*/
void LibsshQtClient::useAuth(UseAuths auths, bool enabled)
{
    if ( enabled ) {
        use_auths_ |= auths;
        if ( state_ == StateAuthChoose || state_ == StateAuthAllFailed ) {
            setState(StateAuthContinue);
            timer_.start();
        }

    } else {
        use_auths_ &= ~auths;
    }
}

/*!
    This enables all authentications you would normally like to use.
*/
void LibsshQtClient::useDefaultAuths()
{
    UseAuths a(UseAuthNone | UseAuthAutoPubKey | UseAuthPassword | UseAuthKbi);
    useAuth(a, true);
}

/*!
    Enable or disable the use of 'None' SSH authentication.
*/
void LibsshQtClient::useNoneAuth(bool enabled)
{
    useAuth(UseAuthNone, enabled);
}

/*!
    Enable or disable the use of automatic public key authentication.

    This includes keys stored in ssh-agent and in ~/.ssh/ directory.
*/
void LibsshQtClient::useAutoKeyAuth(bool enabled)
{
    useAuth(UseAuthAutoPubKey, enabled);
}

/*!
    Enable or disable the use of password based SSH authentication.
*/
void LibsshQtClient::usePasswordAuth(bool enabled)
{
    useAuth(UseAuthPassword, enabled);
}

/*!
    Enable or disable the use of Keyboard Interactive SSH authentication.
*/
void LibsshQtClient::useKbiAuth(bool enabled)
{
    useAuth(UseAuthKbi, enabled);
}

/*!
    Set password for use in password authentication.
*/
void LibsshQtClient::setPassword(QString password)
{
    password_set_ = true;
    password_ = password;

    if ( state_ == StateAuthNeedPassword ) {
        setState(StateAuthPassword);
        timer_.start();
    }
}

/*!
    Get list of Keyboard Interactive questions sent by the server.
*/
QList<LibsshQtClient::KbiQuestion> LibsshQtClient::kbiQuestions()
{
    Q_ASSERT( state_ == StateAuthKbiQuestions );
    if ( state_ == StateAuthKbiQuestions ) {

        QList<KbiQuestion> questions;
        QString instruction = ssh_userauth_kbdint_getinstruction(session_);

        int len = ssh_userauth_kbdint_getnprompts(session_);
        LIBSSHQT_DEBUG("Number of KBI questions:" << len);

        for ( int i = 0; i < len; i++) {

            char echo = 0;
            const char *prompt = 0;

            prompt = ssh_userauth_kbdint_getprompt(session_, i, &echo);
            Q_ASSERT( prompt );

            KbiQuestion kbi_question;
            kbi_question.instruction = instruction;
            kbi_question.question    = QString(prompt);
            kbi_question.showAnswer  = echo != 0;

            LIBSSHQT_DEBUG("KBI Question:" << kbi_question);
            questions << kbi_question;
        }

        Q_ASSERT( questions.count() > 0 );
        return questions;

    } else {
        LIBSSHQT_CRITICAL("Cannot get KBI questions because state is" <<
                          state_);
        return QList<KbiQuestion>();
    }
}

/*!
    Set answers to Keyboard Interactive questions.
*/
void LibsshQtClient::setKbiAnswers(QStringList answers)
{
    Q_ASSERT( state_ == StateAuthKbiQuestions );
    if ( state_ == StateAuthKbiQuestions ) {
        LIBSSHQT_CRITICAL("Setting KBI answers");

        int i = 0;
        foreach ( QString answer, answers ) {
            QByteArray utf8 = answer.toUtf8();
            ssh_userauth_kbdint_setanswer(session_, i, utf8.constData());
        }

        setState(StateAuthKbi);
        timer_.start();

    } else {
        LIBSSHQT_CRITICAL("Cannot set KBI answers because state is" << state_);
    }
}

/*!
    Get supported authentication methods.
*/
LibsshQtClient::AuthMethods LibsshQtClient::supportedAuthMethods()
{
    return AuthMethods(ssh_userauth_list(session_, 0));
}

/*!
    Get all enabled authentication methods.
*/
LibsshQtClient::UseAuths LibsshQtClient::enabledAuths()
{
    return use_auths_;
}

/*!
    Get list of authention methods that have been attempted unsuccesfully.
*/
LibsshQtClient::UseAuths LibsshQtClient::failedAuths()
{
    return failed_auths_;
}

/*!
    Run a command
*/
LibsshQtProcess *LibsshQtClient::runCommand(QString command)
{
    LibsshQtProcess *process = new LibsshQtProcess(this);
    process->setCommand(command);
    process->openChannel();
    return process;
}

LibsshQtClient::HostState LibsshQtClient::unknownHostType()
{
    return unknown_host_type_;
}

/*!
    Get a message to show to the user why the host is unknown.
*/
QString LibsshQtClient::unknownHostMessage()
{
    switch ( unknown_host_type_ ) {

    case HostKnown:
        return QString() +
                "This host is known.";

    case HostUnknown:
    case HostKnownHostsFileMissing:
        return QString() +
                "This host is unknown.";

    case HostKeyChanged:
        return QString() +
                "WARNING: The public key sent by this host does not match the " +
                "expected value. A third party may be attempting to " +
                "impersonate the host.";

    case HostKeyTypeChanged:
        return QString() +
                "WARNING: The public key type sent by this host does not " +
                "match the expected value. A third party may be attempting " +
                "to impersonate the host.";
    }

    Q_ASSERT_X(false, __func__, "Case was not handled properly");
    return QString();
}

/*!
    Get MD5 hexadecimal hash of the servers public key.
*/
QString LibsshQtClient::hostPublicKeyHash()
{
    QString string;
    int hash_len = 0;
    unsigned char *hash = NULL;
    char *hexa = NULL;

    hash_len = ssh_get_pubkey_hash(session_, &hash);
    hexa = ssh_get_hexa(hash, hash_len);

    string = QString(hexa);

    free(hexa);
    free(hash);

    return string;
}

/*!
    Add current host to the known hosts file.
*/
bool LibsshQtClient::markCurrentHostKnown()
{
    int rc = ssh_write_knownhost(session_);

    switch ( rc ) {
    case SSH_OK:
        LIBSSHQT_DEBUG("Added current host to known host list");
        setState(StateIsKnown);
        timer_.start();
        return true;

    case SSH_ERROR:
        LIBSSHQT_DEBUG("Could not add current host to known host list");
        return false;

    default:
        LIBSSHQT_CRITICAL("Unknown result code" << rc <<
                          "received from ssh_write_knownhost()");
        return false;
    }
}

/*!
    Get error code and message from libssh.
*/
QString LibsshQtClient::errorCodeAndMessage() const
{
    return QString("%1: %2").arg(errorCode()).arg(errorMessage());
}

/*!
    Get error message from libssh.
*/
QString LibsshQtClient::errorMessage() const
{
    return QString(ssh_get_error(session_));
}

/*!
    Get error code from libssh.
*/
int LibsshQtClient::errorCode() const
{
    return ssh_get_error_code(session_);
}

LibsshQtClient::State LibsshQtClient::state() const
{
    return state_;
}

ssh_session LibsshQtClient::sshSession()
{
    return session_;
}

void LibsshQtClient::enableWritableNotifier()
{
    if ( process_state_running_ ) {
        enable_writable_nofifier_ = true;
    } else if ( write_notifier_ ) {
        write_notifier_->setEnabled(true);
    }
}

/*!
    Change session state and send appropriate signals.
*/
void LibsshQtClient::setState(State state)
{
    if ( state_ == state ) {
        LIBSSHQT_DEBUG("State is already" << state);
        return;
    }

    LIBSSHQT_DEBUG("Changing state to" << state);
    state_ = state;

    if ( state_ == StateError ) {
        destroyNotifiers();
    }

    // Emit signals
    switch ( state_ ) {
    case StateClosed:           emit closed();                  break;
    case StateClosing:                                          break;
    case StateInit:                                             break;
    case StateConnecting:                                       break;
    case StateIsKnown:                                          break;
    case StateUnknownHost:      emit unknownHost();             break;
    case StateAuthChoose:       emit chooseAuth();              break;
    case StateAuthContinue:                                     break;
    case StateAuthNone:                                         break;
    case StateAuthAutoPubkey:                                   break;
    case StateAuthPassword:                                     break;
    case StateAuthNeedPassword: emit needPassword();            break;
    case StateAuthKbi:                                          break;
    case StateAuthKbiQuestions: emit needKbiAnswers();          break;
    case StateAuthAllFailed:    emit allAuthsFailed();          break;
    case StateOpened:           emit opened();                  break;
    case StateError:            emit error();                   break;
    }
}

/*!
    Choose next authentication method to try.

    if authentication methods have not been chosen or all chosen authentication
    methods have failed, switch state to StateChooseAuth or StateAuthFailed,
    respectively.
*/
void LibsshQtClient::tryNextAuth()
{
    UseAuths failed_auth = UseAuthEmpty;

    // Detect failed authentication methods
    switch ( state_ ) {
    case StateClosed:
    case StateClosing:
    case StateInit:
    case StateConnecting:
    case StateIsKnown:
    case StateUnknownHost:
    case StateAuthChoose:
    case StateAuthContinue:
    case StateAuthNeedPassword:
    case StateAuthKbiQuestions:
    case StateAuthAllFailed:
    case StateOpened:
    case StateError:
        break;

    case StateAuthNone:
        failed_auth = UseAuthNone;
        break;

    case StateAuthAutoPubkey:
        failed_auth = UseAuthAutoPubKey;
        break;

    case StateAuthPassword:
        failed_auth = UseAuthPassword;
        break;

    case StateAuthKbi:
        failed_auth = UseAuthKbi;
        break;
    }

    if ( failed_auth != UseAuthEmpty ) {
        failed_auths_ |= failed_auth;
        State old_state = state_;
        emit authFailed(failed_auth);

        // User might close, or otherwise manipulate, the LibsshQtClient when an
        // auth fails, so make sure that the state has not been changed.
        if ( state_ != old_state ) {
            return;
        }
    }

    LIBSSHQT_DEBUG("Enabled auths:" << use_auths_);
    LIBSSHQT_DEBUG("Failed auths:" << failed_auths_);

    // Choose next state for LibsshQtClient
    if ( use_auths_ == UseAuthEmpty && failed_auths_ == UseAuthEmpty ) {
        setState(StateAuthChoose);

    } else if ( use_auths_ == UseAuthEmpty ) {
        setState(StateAuthAllFailed);

    } else if ( use_auths_ & UseAuthNone ) {
        use_auths_ &= ~UseAuthNone;
        setState(StateAuthNone);
        timer_.start();

    } else if ( use_auths_ & UseAuthAutoPubKey ) {
        use_auths_ &= ~UseAuthAutoPubKey;
        setState(StateAuthAutoPubkey);
        timer_.start();

    } else if ( use_auths_ & UseAuthPassword ) {
        use_auths_ &= ~UseAuthPassword;
        setState(StateAuthPassword);
        timer_.start();

    } else if ( use_auths_ & UseAuthKbi ) {
        use_auths_ &= ~UseAuthKbi;
        setState(StateAuthKbi);
        timer_.start();
    }
}

void LibsshQtClient::setUpNotifiers()
{
    if ( ! read_notifier_ ) {
        socket_t socket = ssh_get_fd(session_);

        LIBSSHQT_DEBUG("Setting up read notifier for socket" << socket);
        read_notifier_ = new QSocketNotifier(socket,
                                             QSocketNotifier::Read,
                                             this);
        read_notifier_->setEnabled(true);
        connect(read_notifier_, SIGNAL(activated(int)),
                this, SLOT(handleSocketReadable(int)));
    }

    if ( ! write_notifier_ ) {
        socket_t socket = ssh_get_fd(session_);

        LIBSSHQT_DEBUG("Setting up write notifier for socket" << socket);
        write_notifier_ = new QSocketNotifier(socket,
                                              QSocketNotifier::Write,
                                              this);
        write_notifier_->setEnabled(true);
        connect(write_notifier_, SIGNAL(activated(int)),
                this, SLOT(handleSocketWritable(int)));
    }
}

void LibsshQtClient::destroyNotifiers()
{
    if ( read_notifier_ ) {
        read_notifier_->disconnect(this);
        read_notifier_->setEnabled(false);
        read_notifier_->deleteLater();
        read_notifier_ = 0;
    }

    if ( write_notifier_ ) {
        write_notifier_->disconnect(this);
        write_notifier_->setEnabled(false);
        write_notifier_->deleteLater();
        write_notifier_ = 0;
    }
}

void LibsshQtClient::handleSocketReadable(int socket)
{
    Q_UNUSED( socket );

    read_notifier_->setEnabled(false);
    processStateGuard();
    if ( read_notifier_ ) {
        read_notifier_->setEnabled(true);
    }
}

void LibsshQtClient::handleSocketWritable(int socket)
{
    Q_UNUSED( socket );

    enable_writable_nofifier_ = false;
    write_notifier_->setEnabled(false);
    processStateGuard();
}

void LibsshQtClient::processStateGuard()
{
    Q_ASSERT( ! process_state_running_ );
    if ( process_state_running_ ) return;

    process_state_running_ = true;
    processState();
    process_state_running_ = false;

    if ( write_notifier_ && enable_writable_nofifier_ ) {
        write_notifier_->setEnabled(true);
    }
}

void LibsshQtClient::processState()
{
    switch ( state_ ) {

    case StateClosed:
    case StateClosing:
    case StateUnknownHost:
    case StateAuthChoose:
    case StateAuthNeedPassword:
    case StateAuthKbiQuestions:
    case StateAuthAllFailed:
    case StateError:
        return;

    case StateInit:
    {
        Q_ASSERT( session_ == 0 );

        session_ = ssh_new();
        if ( ! session_ ) {
            LIBSSHQT_FATAL("Could not create SSH session");
        }
        ssh_set_blocking(session_, 0);

        unsigned int tmp_port = port_;
        int tmp_verbosity = log_verbosity_;

        if (setLibsshOption(SSH_OPTIONS_LOG_VERBOSITY,
                            "SSH_OPTIONS_LOG_VERBOSITY",
                            &tmp_verbosity, enumToString(log_verbosity_)) &&

            setLibsshOption(SSH_OPTIONS_USER, "SSH_OPTIONS_USER",
                            qPrintable(username_), username_) &&

            setLibsshOption(SSH_OPTIONS_HOST, "SSH_OPTIONS_HOST",
                            qPrintable(hostname_), hostname_) &&

            setLibsshOption(SSH_OPTIONS_PORT, "SSH_OPTIONS_PORT",
                            &tmp_port, QString::number(port_)))
        {
            setState(StateConnecting);
            timer_.start();
            return;
        }

        return;
    } break;

    case StateConnecting:
    {
        int rc = ssh_connect(session_);
        if ( rc != SSH_ERROR &&
             ( read_notifier_ == 0 || write_notifier_ == 0 )) {
            setUpNotifiers();
        }

        switch ( rc ) {
        case SSH_AGAIN:
            enableWritableNotifier();
            return;

        case SSH_ERROR:
            LIBSSHQT_DEBUG("Channel open error:" << errorCodeAndMessage());
            setState(StateError);
            return;

        case SSH_OK:
            setState(StateIsKnown);
            timer_.start();
            return;

        default:
            LIBSSHQT_CRITICAL("Unknown result code" << rc <<
                              "received from ssh_connect()");
            return;
        }
    } break;

    case StateIsKnown:
    {
        int known = ssh_is_server_known(session_);

        switch ( known ) {
        case SSH_SERVER_ERROR:
            setState(StateError);
            return;

        case SSH_SERVER_NOT_KNOWN:
        case SSH_SERVER_KNOWN_CHANGED:
        case SSH_SERVER_FOUND_OTHER:
        case SSH_SERVER_FILE_NOT_FOUND:
            unknown_host_type_ = static_cast< HostState >( known );
            LIBSSHQT_DEBUG("Setting unknown host state to" <<
                           unknown_host_type_);
            setState(StateUnknownHost);
            return;

        case SSH_SERVER_KNOWN_OK:
            unknown_host_type_ = HostKnown;
            tryNextAuth();
            return;

        default:
            LIBSSHQT_CRITICAL("Unknown result code" << known <<
                              "received from ssh_is_server_known()");
            return;
        }
    } break;

    case StateAuthContinue: {
        tryNextAuth();
        return;
    } break;

    case StateAuthNone:
    {
        int rc = ssh_userauth_none(session_, 0);
        handleAuthResponse(rc, "ssh_userauth_none", UseAuthNone);
        return;
    } break;

    case StateAuthAutoPubkey:
    {
        int rc = ssh_userauth_autopubkey(session_, 0);
        handleAuthResponse(rc, "ssh_userauth_autopubkey", UseAuthAutoPubKey);
        return;
    } break;

    case StateAuthPassword:
    {
        int status = ssh_get_status(session_);
        if ( status == SSH_CLOSED ||
             status == SSH_CLOSED_ERROR) {
            setState(StateError);
            return;

        } else if ( ! password_set_ ) {
            setState(StateAuthNeedPassword);
            return;

        } else {
            QByteArray utf8pw = password_.toUtf8();
            int rc = ssh_userauth_password(session_, 0, utf8pw.constData());

            if ( rc != SSH_AUTH_AGAIN ) {
                password_set_ = false;
                password_.clear();
            }

            handleAuthResponse(rc, "ssh_userauth_password", UseAuthPassword);
            return;
        }
    } break;

    case StateAuthKbi: {
        int rc = ssh_userauth_kbdint(session_, 0, 0);
        if ( rc == SSH_AUTH_INFO ) {

             // Sometimes SSH_AUTH_INFO is returned even though there are no
             // KBI questions available, in that case, continue as if
             // SSH_AUTH_AGAIN was returned.
             if ( ssh_userauth_kbdint_getnprompts(session_) <= 0 ) {
                 enableWritableNotifier();

             } else {
                 setState(StateAuthKbiQuestions);
             }

        } else {
            handleAuthResponse(rc, "ssh_userauth_kbdint", UseAuthKbi);
        }
        return;
    }

    case StateOpened:
    {
        int status = ssh_get_status(session_);
        if ( status == SSH_CLOSED ||
             status == SSH_CLOSED_ERROR) {
            setState(StateError);
            return;

        } else {
            // Activate processState() function on all children so that they can
            // process their events and read and write IO.
            emit doProcessState();
            return;
        }
    } break;

    } // End switch

    Q_ASSERT_X(false, __func__, "Case was not handled properly");
}

void LibsshQtClient::handleAuthResponse(int         rc,
                                        const char *func,
                                        UseAuthFlag auth)
{
    switch ( rc ) {
    case SSH_AUTH_AGAIN:
        enableWritableNotifier();
        return;

    case SSH_AUTH_ERROR:
        LIBSSHQT_DEBUG("Authentication error:" << auth <<
                       errorCodeAndMessage());
        setState(StateError);
        return;

    case SSH_AUTH_DENIED:
        LIBSSHQT_DEBUG("Authentication denied:" << auth);
        tryNextAuth();
        return;

    case SSH_AUTH_PARTIAL:
        LIBSSHQT_DEBUG("Partial authentication:" << auth);
        tryNextAuth();
        return;

    case SSH_AUTH_SUCCESS:
        LIBSSHQT_DEBUG("Authentication success:" << auth);
        succeeded_auth_ = auth;
        setState(StateOpened);
        timer_.start();
        return;

    default:
        LIBSSHQT_CRITICAL("Unknown result code" << rc <<
                          "received from" << func);
        return;
    }

    Q_ASSERT_X(false, __func__, "Case was not handled properly");
}

bool LibsshQtClient::setLibsshOption(enum ssh_options_e type,
                                     QString type_debug,
                                     const void *value,
                                     QString value_debug)
{
    Q_ASSERT( session_ != 0 );

    if ( state_ == StateError ) {
        LIBSSHQT_CRITICAL("Cannot set option" << type_debug <<
                          "to" << value_debug << "because state is" << state_);
        return false;
    }

    LIBSSHQT_DEBUG("Setting option" << type_debug << "to" << value_debug);
    if ( ssh_options_set(session_, type, value) != 0 ) {
        LIBSSHQT_CRITICAL("Failed to set option " << type_debug <<
                          "to" << value_debug);
        setState(StateError);
        return false;
    }

    return true;
}























