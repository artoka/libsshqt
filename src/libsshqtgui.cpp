
#include <QDebug>
#include <QMetaEnum>
#include <QIcon>
#include <QTextDocument>

#include "libsshqtgui.h"
#include "ui_libsshqtgui.h"

#include "libsshqtdebug.h"
#include "libsshqt.h"

LibsshQtGui::LibsshQtGui(QWidget *parent) :
    QDialog(parent),
    debug_output_(false),
    ui_(new Ui::libsshqtgui),
    client_(0),
    state_(StateHidden),
    kbi_pos_(0)
{
    debug_prefix_ = LibsshQt::debugPrefix(this);
    ui_->setupUi(this);
    ui_->host_widget->hide();
    ui_->auth_widget->hide();

    setMinimumSize(450, 150);
    setMaximumSize(640, 480);

    QSize size(64, 64);
    QIcon warning  = style()->standardIcon(QStyle::SP_MessageBoxWarning);
    QIcon question = style()->standardIcon(QStyle::SP_MessageBoxQuestion);

    setUnknownHostIcon(warning.pixmap(size));
    setPasswordIcon(question.pixmap(size));
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
    ui_->auth_icon_label->setPixmap(pixmap);
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
            client_->setPassword(ui_->auth_line->text());
            break;

        case StateKbiAuthDlg:
            kbi_answers_ << ui_->auth_line->text();

            if ( kbi_pos_ < kbi_questions_.count()) {
                QTimer::singleShot(0, this, SLOT(showNextKbiQuestion()));

            } else {
                client_->setKbiAnswers(kbi_answers_);
                kbi_questions_.clear();
                kbi_answers_.clear();
                kbi_pos_ = 0;
            }
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

    QString msg = QString("%1 %2")
            .arg(Qt::escape(client_->unknownHostMessage()))
            .arg(tr("Do you want to trust this host?"));

    QString info = QString("<br>%1: <tt>%2:%3</tt><br>%4: <tt>%5</tt>")
            .arg(Qt::escape(tr("Hostname")))
            .arg(Qt::escape(client_->hostname()))
            .arg(client_->port())
            .arg(Qt::escape(tr("Key fingerprint")))
            .arg(Qt::escape(client_->hostPublicKeyHash()));

    showHostDlg(msg, info);
}

void LibsshQtGui::handleNeedPassword()
{
    setState(StatePasswordAuthDlg);
    QString question = QString(tr("Type password for %1@%2"))
                       .arg(client_->username())
                       .arg(client_->hostname());
    showAuthDlg(question);
}

void LibsshQtGui::handleNeedKbiAnswers()
{
    setState(StateKbiAuthDlg);

    kbi_pos_       = 0;
    kbi_questions_ = client_->kbiQuestions();

    Q_ASSERT( kbi_questions_.count() > 0 );
    showNextKbiQuestion();
}

void LibsshQtGui::showNextKbiQuestion()
{
    if ( kbi_pos_ < kbi_questions_.count()) {
        LibsshQtClient::KbiQuestion question = kbi_questions_.at(kbi_pos_);
        LIBSSHQT_DEBUG("Showing KBI question" << kbi_pos_ + 1 << "/" <<
                       kbi_questions_.count() << ":" << question);

        QString msg = QString("%1 <tt>%2:%3</tt><br><br>%4")
                           .arg(Qt::escape(tr("Authentication question from")))
                           .arg(Qt::escape(client_->hostname()))
                           .arg(client_->port())
                           .arg(Qt::escape(question.question));
        if ( ! question.instruction.isEmpty()) {
            msg += "<br><br>" + Qt::escape(question.instruction);
        }

        kbi_pos_++;
        showAuthDlg(msg, question.showAnswer);

    } else {
        LIBSSHQT_DEBUG("All KBI questions have been asked already");
    }
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

void LibsshQtGui::showHostDlg(QString message, QString info)
{
    setWindowTitle(tr("Unknown Host"));

    ui_->auth_widget->hide();
    ui_->host_widget->show();
    ui_->host_msg_label->setText(message);
    ui_->host_info_label->setText(info);
    ui_->button_box->setStandardButtons(QDialogButtonBox::Yes |
                                        QDialogButtonBox::No);

    resize(sizeHint());
    show();
}

void LibsshQtGui::showAuthDlg(QString message, bool show_answer)
{
    setWindowTitle(tr("Authentication"));

    ui_->host_widget->hide();
    ui_->auth_widget->show();
    ui_->auth_msg_label->setText(message);
    ui_->auth_line->clear();
    ui_->button_box->setStandardButtons(QDialogButtonBox::Ok |
                                        QDialogButtonBox::Cancel);

    if ( show_answer ) {
        ui_->auth_line->setEchoMode(QLineEdit::Normal);
    } else {
        ui_->auth_line->setEchoMode(QLineEdit::Password);
    }

    resize(sizeHint());
    show();
}




























