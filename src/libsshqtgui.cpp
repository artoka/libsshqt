
#include <QDebug>
#include <QMetaEnum>
#include <QIcon>
#include <QTextDocument>

#include "libsshqtgui.h"
#include "ui_libsshqtgui.h"

#include "libsshqtdebug.h"
#include "libsshqt.h"

static const QSize default_size(450, 150);

LibsshQtGui::LibsshQtGui(QWidget *parent) :
    QDialog(parent),
    debug_output_(false),
    ui_(new Ui::libsshqtgui),
    client_(0),
    state_(StateHidden)
{
    debug_prefix_ = LibsshQt::debugPrefix(this);
    ui_->setupUi(this);

    QSize size(64, 64);
    QIcon warning  = style()->standardIcon(QStyle::SP_MessageBoxWarning);
    QIcon question = style()->standardIcon(QStyle::SP_MessageBoxQuestion);

    setUnknownHostIcon(warning.pixmap(size));
    setPasswordIcon(question.pixmap(size));

    resize(default_size);
}

LibsshQtGui::~LibsshQtGui()
{
    delete ui_;
}

const char *LibsshQtGui::enumToString(const State value)
{
    return staticMetaObject.enumerator(
                staticMetaObject.indexOfEnumerator("State"))
                    .valueToKey(value);
}

void LibsshQtGui::setClient(LibsshQtClient *client)
{
    if ( client_ ) {
        client_->disconnect(this);
    }

    client_ = client;
    debug_output_ = client->isDebugEnabled();
    LIBSSHQT_DEBUG("Client set to:" <<
                   qPrintable(LibsshQt::hexAndName(client)));

    connect(client_, SIGNAL(unknownHost()),    this, SLOT(handleUnknownHost()));
    connect(client_, SIGNAL(authFailed()),     this, SLOT(handleAuthFailed()));
    connect(client_, SIGNAL(needPassword()),   this, SLOT(handleNeedPassword()));
    connect(client_, SIGNAL(needKbiAnswers()), this, SLOT(handleNeedKbiAnswers()));

    connect(client_, SIGNAL(closed()), this, SLOT(hide()));
    connect(client_, SIGNAL(opened()), this, SLOT(hide()));
    connect(client_, SIGNAL(error()),  this, SLOT(hide()));

    if ( client->state() == LibsshQtClient::StateUnknownHost ) {
        handleUnknownHost();

    } else if ( client->state() == LibsshQtClient::StateAuthNeedPassword ) {
        handleNeedPassword();

    } else if ( client->state() == LibsshQtClient::StateAuthKbiQuestions ) {
        handleNeedKbiAnswers();

    } else if ( client->state() == LibsshQtClient::StateAuthFailed ) {
        handleAuthFailed();
    }
}

void LibsshQtGui::setUnknownHostIcon(QPixmap pixmap)
{
    ui_->host_icon_label->setPixmap(pixmap);
}

void LibsshQtGui::setPasswordIcon(QPixmap pixmap)
{
    ui_->question_icon_label->setPixmap(pixmap);
}

LibsshQtGui::State LibsshQtGui::state() const
{
    return state_;
}

void LibsshQtGui::done(int code)
{
    if ( code == QDialog::Accepted) {
        LIBSSHQT_DEBUG("Dialog accepted in state:" << state_);

        switch ( state_ ) {
        case StateHidden:
            // What are we doing here?
            break;

        case StateUnknownHostDlg:
            LIBSSHQT_DEBUG("Accepting host");
            client_->markCurrentHostKnown();
            break;

        case StatePasswordAuthDlg:
            LIBSSHQT_DEBUG("Setting password");
            client_->setPassword(ui_->question_line->text());
            break;

        case StateKbiAuthDlg:
            //! @todo
            break;
        }

    } else {
        LIBSSHQT_DEBUG("Dialog rejected in state:" << state_);
        LIBSSHQT_DEBUG("Closing connection:" << LIBSSHQT_HEXNAME(client_));
        client_->disconnectFromHost();
    }

    QDialog::done(code);
}

void LibsshQtGui::setVisible(bool visible)
{
    QDialog::setVisible(visible);

    if ( visible ) {
        Q_ASSERT( state_ != StateHidden );
    } else {
        setState(StateHidden);
    }
}

void LibsshQtGui::handleUnknownHost()
{
    LIBSSHQT_DEBUG("Handling unknown host");
    setState(StateUnknownHostDlg);
    showHostPage(client_->unknownHostMessage(),
                 client_->hostname() + ":" + QString::number(client_->port()),
                 client_->hostPublicKeyHash());
}

void LibsshQtGui::handleNeedPassword()
{
    setState(StatePasswordAuthDlg);
    QString question = QString(tr("Type password for %1@%2"))
                       .arg(client_->username())
                       .arg(client_->hostname());
    showQuestionPage(question);
}

void LibsshQtGui::handleNeedKbiAnswers()
{
    setState(StateKbiAuthDlg);
    //! @todo
}

void LibsshQtGui::handleAuthFailed()
{
    LibsshQtClient::AuthMethods supported = client_->supportedAuthMethods();
    LibsshQtClient::UseAuths    failed    = client_->failedAuths();

    if ( failed & LibsshQtClient::UseAuthPassword &&
         supported &  LibsshQtClient::AuthMethodPassword ) {
        LIBSSHQT_DEBUG("Retrying:" << LibsshQtClient::AuthMethodPassword);
        client_->usePasswordAuth(true);

    } else if ( failed & LibsshQtClient::UseAuthKbi &&
                supported & LibsshQtClient::AuthMethodKbi ) {
        LIBSSHQT_DEBUG("Retrying:" << LibsshQtClient::UseAuthKbi);
        client_->useKbiAuth(true);

    } else {
        LIBSSHQT_DEBUG("Supported auth:" << supported);
        LIBSSHQT_DEBUG("Failed auths:" << failed);
        LIBSSHQT_DEBUG("Cannot find an authentication method that is enabled" <<
                       "by the user and supported by the host");
        LIBSSHQT_DEBUG("Closing connection:" << LIBSSHQT_HEXNAME(client_));
        client_->disconnect();
    }
}

void LibsshQtGui::setState(State state)
{
    if ( state_ == state ) {
        LIBSSHQT_DEBUG("State is already" << state);
        return;
    }

    LIBSSHQT_DEBUG("Changing state to" << state);
    state_ = state;
}

void LibsshQtGui::showHostPage(QString message, QString hostname, QString key)
{
    static const char *format = "<tt>%1<tt>";

    hostname = QString(format).arg(Qt::escape(hostname));
    key = QString(format).arg(Qt::escape(key));

    setWindowTitle(tr("Unknown Host"));
    ui_->pages->setCurrentIndex(0);
    ui_->host_msg_label->setText(message);
    ui_->host_name_label->setText(hostname);
    ui_->host_pubkey_label->setText(key);

    ui_->question_label->clear();

    resize(default_size);
    show();
}

void LibsshQtGui::showQuestionPage(QString question)
{
    setWindowTitle(tr("Credentials needed"));

    ui_->pages->setCurrentIndex(1);
    ui_->question_label->setText(question);
    ui_->question_line->clear();
    ui_->question_line->setEchoMode(QLineEdit::Password);

    ui_->host_msg_label->clear();
    ui_->host_name_label->clear();
    ui_->host_pubkey_label->clear();

    resize(default_size);
    show();
}

























