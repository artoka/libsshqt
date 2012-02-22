#ifndef LIBSSHQTGUI_H
#define LIBSSHQTGUI_H

#include <QDialog>

#include "libsshqt.h"

namespace Ui {
    class libsshqtgui;
}

class LibsshQtGui : public QDialog
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

    explicit LibsshQtGui(QWidget *parent = 0);
    ~LibsshQtGui();

    static const char *enumToString(const State value);

    void setClient(LibsshQtClient *client);
    void setUnknownHostIcon(QIcon icon);
    void setAuthIcon(QIcon icon);
    State state() const;

    void done(int code);
    void setVisible(bool visible);

private slots:
    void handleUnknownHost();
    void handleNeedPassword();
    void handleNeedKbiAnswers();
    void showNextKbiQuestion();
    void handleAuthFailed();

private:
    void setState(State state);
    void showHostDlg(QString message, QString info);
    void showAuthDlg(QString message, bool show_answer = false);

private:
    QString          debug_prefix_;
    bool             debug_output_;

    Ui::libsshqtgui *ui_;
    LibsshQtClient  *client_;
    State            state_;

    QList<LibsshQtClient::KbiQuestion> kbi_questions_;
    QStringList                        kbi_answers_;
    int                                kbi_pos_;
};

// Include <QDebug> before "libsshqt.h" if you want to use these operators
#ifdef QDEBUG_H

inline QDebug operator<<(QDebug dbg, const LibsshQtGui *gui)
{
    dbg.nospace() << "LibsshQtGui( "
                  << LibsshQtGui::enumToString(gui->state())
                  << " )";
    return dbg.space();
}

inline QDebug operator<<(QDebug dbg, const LibsshQtGui::State value)
{
    dbg << LibsshQtGui::enumToString(value);
    return dbg;
}

#endif

#endif // LIBSSHQTGUI_H

































