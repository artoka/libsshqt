
#include <QDebug>
#include <QMetaEnum>
#include <QIcon>
#include <QMessageBox>
#include <QTextDocument>

#include "libsshqtquestiondialog.h"
#include "ui_libsshqtquestiondialog.h"

#include "libsshqtdebug.h"

static const QSize icon_size(64, 64);
static const QSize min_size(450, 150);
static const QSize max_size(640, 480);

LibsshQtQuestionDialog::LibsshQtQuestionDialog(QWidget *parent) :
    QDialog(parent),
    debug_output_(false),
    ui_(new Ui::LibsshQtQuestionDialog),
    client_(0),
    state_(StateHidden),
    kbi_pos_(0),
    show_auths_failed_dlg_(true),
    show_error_dlg_(true)
{
    debug_prefix_ = LibsshQt::debugPrefix(this);

    ui_->setupUi(this);
    ui_->host_widget->hide();
    ui_->auth_widget->hide();

    setUnknownHostIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
    setAuthIcon(style()->standardIcon(QStyle::SP_MessageBoxQuestion));
}

LibsshQtQuestionDialog::~LibsshQtQuestionDialog()
{
    delete ui_;
}

const char *LibsshQtQuestionDialog::enumToString(const State value)
{
    return staticMetaObject.enumerator(
                staticMetaObject.indexOfEnumerator("State"))
                    .valueToKey(value);
}

/*!
    Set which LibsshQtClient object is handled by LibsshQtQuestionDialog.
*/
void LibsshQtQuestionDialog::setClient(LibsshQtClient *client)
{
    if ( client_ ) {
        client_->disconnect(this);
    }

    hide();
    client_ = client;
    debug_output_ = client->isDebugEnabled();
    LIBSSHQT_DEBUG("Client set to:" <<
                   qPrintable(LibsshQt::hexAndName(client)));

    connect(client_, SIGNAL(debugChanged()),
            this,    SLOT(handleDebugChanged()));
    connect(client_, SIGNAL(unknownHost()),
            this,    SLOT(handleUnknownHost()));
    connect(client_, SIGNAL(authFailed(int)),
            this,    SLOT(handleAuthFailed(int)));
    connect(client_, SIGNAL(allAuthsFailed()),
            this,    SLOT(handleAllAuthsFailed()));
    connect(client_, SIGNAL(needPassword()),
            this,    SLOT(handleNeedPassword()));
    connect(client_, SIGNAL(needKbiAnswers()),
            this,    SLOT(handleNeedKbiAnswers()));
    connect(client_, SIGNAL(error()),
            this,    SLOT(handleError()));

    connect(client_, SIGNAL(closed()), this, SLOT(hide()));
    connect(client_, SIGNAL(opened()), this, SLOT(hide()));
    connect(client_, SIGNAL(error()),  this, SLOT(hide()));

    if ( client->state() == LibsshQtClient::StateUnknownHost ) {
        handleUnknownHost();

    } else if ( client->state() == LibsshQtClient::StateAuthNeedPassword ) {
        handleNeedPassword();

    } else if ( client->state() == LibsshQtClient::StateAuthKbiQuestions ) {
        handleNeedKbiAnswers();

    } else if ( client->state() == LibsshQtClient::StateAuthAllFailed ) {
        handleAllAuthsFailed();
    }
}

/*!
    Set which icon is used in "Unknown Host" dialogs.

    Defaults to QStyle::SP_MessageBoxWarning
*/
void LibsshQtQuestionDialog::setUnknownHostIcon(QIcon icon)
{
    ui_->host_icon_label->setPixmap(icon.pixmap(icon_size));
}

/*!
    Set which icon is used in "Authentication" dialogs.

    Defaults to QStyle::SP_MessageBoxQuestion
*/
void LibsshQtQuestionDialog::setAuthIcon(QIcon icon)
{
    ui_->auth_icon_label->setPixmap(icon.pixmap(icon_size));
}

/*!
    Should "Authentication failed" dialog be show when LibsshQtClient emits
    allAuthsFailed signal.
*/
void LibsshQtQuestionDialog::setShowAuthsFailedDlg(bool enabled)
{
    show_auths_failed_dlg_ = enabled;
}

/*!
    Should "Error" dialog be show when LibsshQtClient emits error signal.
*/
void LibsshQtQuestionDialog::setShowErrorDlg(bool enabled)
{
    show_error_dlg_ = enabled;
}

LibsshQtQuestionDialog::State LibsshQtQuestionDialog::state() const
{
    return state_;
}

void LibsshQtQuestionDialog::done(int code)
{
    Q_ASSERT( state_ != StateHidden );

    if ( state_ == StateHidden ) {
        LIBSSHQT_CRITICAL("done() called in state:" << state_);

    } else if ( code == QDialog::Accepted) {
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

void LibsshQtQuestionDialog::setVisible(bool visible)
{
    QDialog::setVisible(visible);

    if ( visible ) {
        Q_ASSERT( state_ != StateHidden );

    } else if ( state_ != StateHidden ) {

        // Clear dialog for reuse
        ui_->host_info_label->clear();
        ui_->host_msg_label->clear();
        ui_->host_widget->hide();
        ui_->auth_msg_label->clear();
        ui_->auth_line->clear();
        ui_->auth_widget->hide();

        setState(StateHidden);
    }
}

void LibsshQtQuestionDialog::handleDebugChanged()
{
    debug_output_ = client_->isDebugEnabled();
}

void LibsshQtQuestionDialog::handleUnknownHost()
{
    Q_ASSERT( state_ == StateHidden );
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

void LibsshQtQuestionDialog::handleNeedPassword()
{
    Q_ASSERT( state_ == StateHidden );
    LIBSSHQT_DEBUG("Handling password input");
    setState(StatePasswordAuthDlg);

    QString question = QString(tr("Type password for %1@%2"))
                       .arg(client_->username())
                       .arg(client_->hostname());
    showAuthDlg(question);
}

void LibsshQtQuestionDialog::handleNeedKbiAnswers()
{
    Q_ASSERT( state_ == StateHidden );
    LIBSSHQT_DEBUG("Handling KBI answer input");
    setState(StateKbiAuthDlg);

    kbi_pos_       = 0;
    kbi_questions_ = client_->kbiQuestions();

    showNextKbiQuestion();
}

void LibsshQtQuestionDialog::showNextKbiQuestion()
{
    Q_ASSERT( state_ == StateHidden || state_ == StateKbiAuthDlg );
    Q_ASSERT( kbi_questions_.count() > 0 );
    Q_ASSERT( kbi_pos_ < kbi_questions_.count());

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

void LibsshQtQuestionDialog::handleAuthFailed(int auth)
{
    LibsshQtClient::AuthMethods supported = client_->supportedAuthMethods();

    if ( auth      & LibsshQtClient::UseAuthPassword &&
         supported & LibsshQtClient::AuthMethodPassword ) {
        LIBSSHQT_DEBUG("Retrying:" << LibsshQtClient::AuthMethodPassword);
        client_->usePasswordAuth(true);

    } else if ( auth      & LibsshQtClient::UseAuthKbi &&
                supported & LibsshQtClient::AuthMethodKbi ) {
        LIBSSHQT_DEBUG("Retrying:" << LibsshQtClient::UseAuthKbi);
        client_->useKbiAuth(true);
    }
}

void LibsshQtQuestionDialog::handleAllAuthsFailed()
{
    LIBSSHQT_DEBUG("All authentication attempts have failed");
    LIBSSHQT_DEBUG("Supported auth:" << client_->supportedAuthMethods());
    LIBSSHQT_DEBUG("Failed auths:" << client_->failedAuths());

    if ( show_auths_failed_dlg_ ) {
        QString message = QString("%1 %2:%3")
                .arg(tr("Could not authenticate to"))
                .arg(client_->hostname())
                .arg(client_->port());
        showInfoDlg(message, tr("Authentication failed"));
    }
}

void LibsshQtQuestionDialog::handleError()
{
    if ( show_error_dlg_ ) {
        QString message = QString("%1: %2")
                .arg(tr("Error"))
                .arg(client_->errorMessage());
        showInfoDlg(message, tr("Error"));
    }
}

void LibsshQtQuestionDialog::setState(State state)
{
    if ( state_ == state ) {
        LIBSSHQT_DEBUG("State is already" << state);
        return;
    }

    LIBSSHQT_DEBUG("Changing state to" << state);
    state_ = state;
}

void LibsshQtQuestionDialog::showHostDlg(QString message, QString info)
{
    setWindowTitle(tr("Unknown Host"));

    ui_->host_widget->show();
    ui_->host_msg_label->setText(message);
    ui_->host_info_label->setText(info);
    ui_->button_box->setStandardButtons(QDialogButtonBox::Yes |
                                        QDialogButtonBox::No);

    QSize size = sizeHint().boundedTo(max_size).expandedTo(min_size);
    setMinimumSize(size);
    setMaximumSize(size);
    show();
}

void LibsshQtQuestionDialog::showAuthDlg(QString message, bool show_answer)
{
    setWindowTitle(tr("Authentication"));

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

    QSize size = sizeHint().boundedTo(max_size).expandedTo(min_size);
    setMinimumSize(size);
    setMaximumSize(size);
    show();
}

void LibsshQtQuestionDialog::showInfoDlg(QString message, QString title)
{
    QMessageBox *msg_box = new QMessageBox(parentWidget());
    msg_box->setWindowModality(windowModality());
    msg_box->setWindowTitle(title);
    msg_box->setText(message);
    msg_box->setIcon(QMessageBox::Information);

    connect(msg_box, SIGNAL(accepted()), msg_box, SLOT(deleteLater()));
    connect(msg_box, SIGNAL(rejected()), msg_box, SLOT(deleteLater()));

    msg_box->show();
}




























