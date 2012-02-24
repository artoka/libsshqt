#ifndef LIBSSHQTQUESTIONCONSOLE_H
#define LIBSSHQTQUESTIONCONSOLE_H

#include <QStringList>

#include "libsshqt.h"

class LibsshQtQuestionConsole : public QObject
{
    Q_OBJECT

public:
    explicit LibsshQtQuestionConsole(LibsshQtClient *parent);

private slots:
    void handleUnknownHost();
    void handleNeedPassword();
    void handleNeedKbiAnswers();
    void handleAuthFailed(int auth);
    void handleAllAuthsFailed();
    void handleError();

private:
    QString readLine();
    QString readPassword();

private:
    QString         debug_prefix_;
    bool            debug_output_;
    LibsshQtClient *client_;
};

#endif // LIBSSHQTQUESTIONCONSOLE_H
