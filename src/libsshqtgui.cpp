
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
    setUnknownHostIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(size));
    setPasswordIcon(style()->standardIcon(QStyle::SP_MessageBoxQuestion).pixmap(size));

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

    connect(client_, SIGNAL(unknownHost()), this, SLOT(handleUnknownHost()));
    connect(client_, SIGNAL(chooseAuth()),  this, SLOT(handleChooseAuth()));
    connect(client_, SIGNAL(authFailed()),  this, SLOT(handleAuthFailed()));

    connect(client_, SIGNAL(closed()),      this, SLOT(hide()));
    connect(client_, SIGNAL(opened()),      this, SLOT(hide()));
    connect(client_, SIGNAL(error()),       this, SLOT(hide()));

    if ( client->state() == LibsshQtClient::StateUnknownHost ) {
        handleUnknownHost();

    } else if ( client->state() == LibsshQtClient::StateAuthChoose ) {
        handleChooseAuth();

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
    LIBSSHQT_DEBUG("Dialog finished with code" << code << "in state" << state_);

    if ( code == QDialog::Accepted) {
        switch ( state_ ) {
        case StateHidden:
            // What are we doing here?
            break;

        case StateUnknownHostDlg:
            LIBSSHQT_DEBUG("User accepted the host");
            client_->markCurrentHostKnown();
            break;

        case StatePasswordAuthDlg:
            LIBSSHQT_DEBUG("Enabling password authentication");
            client_->usePasswordAuth(true, ui_->question_line->text());
            break;

        case StateKbiAuthDlg:
            //! @todo
            break;
        }

    } else {
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

void LibsshQtGui::handleChooseAuth()
{
    LIBSSHQT_DEBUG("Handling choose auth");

    LibsshQtClient::AuthMethods supported = client_->supportedAuthMethods();
    LibsshQtClient::UseAuths    failed    = client_->failedAuths();

    if ( supported == LibsshQtClient::AuthMethodUnknown &&
         ! ( failed & LibsshQtClient::UseAuthNone )) {
        setState(StateHidden);
        client_->useNoneAuth(true);

    } else if ( supported &  LibsshQtClient::AuthMethodPassword ) {
        setState(StatePasswordAuthDlg);
        QString question = QString(tr("Type password for %1@%2"))
                           .arg(client_->username())
                           .arg(client_->hostname());
        showQuestionPage(question);

    } else if ( supported & LibsshQtClient::AuthMethodKbi ) {
        setState(StateKbiAuthDlg);
        client_->useKbiAuth(true);
    }
}

void LibsshQtGui::handleAuthFailed()
{
    LIBSSHQT_DEBUG("Handling failed auth");
    handleChooseAuth();
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

























