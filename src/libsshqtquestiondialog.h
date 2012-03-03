#ifndef LIBSSHQTQUESTIONDIALOG_H
#define LIBSSHQTQUESTIONDIALOG_H

#include <QDialog>
#include "libsshqtclient.h"

namespace Ui {
    class LibsshQtQuestionDialog;
}

/*!

    LibsshQtQuestionDialog - Question and message dialog for LibsshQtClient

    LibsshQtQuestionDialog implements the standard question dialogs needed while
    connecting to a SSH server. Specifically LibsshQtQuestionDialog handles
    unknownHost, needPassword and needKbiAnswers signals emitted by
    LibsshQtClient and displays the appropriate dialog to the user.

    If the user types the wrong password then LibsshQtQuestionDialog re-enables
    the attempted authentication method and re-displays the authentication
    dialog. Specifically If LibsshQtClient emits authFailed signal and the
    failed authentication type is either UseAuthPassword or UseAuthKbi and if
    the server actually supports the attempted method, then
    LibsshQtQuestionDialog will re-enable the failed authentication method.

    If LibsshQtClient emits allAuthsFailed or error signal then
    LibsshQtQuestionDialog displays either "Authentication Failed" or "Error"
    messagebox. If you want to display these messages yourself, you can disable
    these messageboxes with setShowAuthsFailedDlg() and setShowErrorDlg()
    functions.

    If user cancels a dialog, LibsshQtClient::disconnectFromHost() is called.

    Use setClient() function to set which LibsshQtClient object is handled by
    LibsshQtQuestionDialog.

*/
class LibsshQtQuestionDialog : public QDialog
{
    Q_OBJECT

public:

    Q_ENUMS(State)
    enum State
    {
        StateHidden,
        StateUnknownHostDlg,
        StatePasswordAuthDlg,
        StateKbiAuthDlg
    };

    explicit LibsshQtQuestionDialog(QWidget *parent = 0);
    ~LibsshQtQuestionDialog();

    static const char *enumToString(const State value);

    void setClient(LibsshQtClient *client);
    void setUnknownHostIcon(QIcon icon);
    void setAuthIcon(QIcon icon);
    void setShowAuthsFailedDlg(bool enabled);
    void setShowErrorDlg(bool enabled);
    State state() const;

    void done(int code);
    void setVisible(bool visible);

private slots:
    void handleDebugChanged();
    void handleUnknownHost();
    void handleNeedPassword();
    void handleNeedKbiAnswers();
    void showNextKbiQuestion();
    void handleAuthFailed(int auth);
    void handleAllAuthsFailed();
    void handleError();

private:
    void setState(State state);
    void showHostDlg(QString message, QString info);
    void showAuthDlg(QString message, bool show_answer = false);
    void showInfoDlg(QString message, QString title);

private:
    QString                             debug_prefix_;
    bool                                debug_output_;

    Ui::LibsshQtQuestionDialog         *ui_;
    LibsshQtClient                     *client_;
    State                               state_;

    QList<LibsshQtClient::KbiQuestion>  kbi_questions_;
    QStringList                         kbi_answers_;
    int                                 kbi_pos_;

    bool                                show_auths_failed_dlg_;
    bool                                show_error_dlg_;
};

// Include <QDebug> before "libsshqt.h" if you want to use these operators
#ifdef QDEBUG_H

inline QDebug operator<<(QDebug dbg, const LibsshQtQuestionDialog *gui)
{
    dbg.nospace() << "LibsshQtQuestionDialog( "
                  << LibsshQtQuestionDialog::enumToString(gui->state())
                  << " )";
    return dbg.space();
}

inline QDebug operator<<(QDebug dbg, const LibsshQtQuestionDialog::State value)
{
    dbg << LibsshQtQuestionDialog::enumToString(value);
    return dbg;
}

#endif

#endif // LIBSSHQTQUESTIONDIALOG_H

































