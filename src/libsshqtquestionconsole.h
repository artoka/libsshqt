#ifndef LIBSSHQTQUESTIONCONSOLE_H
#define LIBSSHQTQUESTIONCONSOLE_H

#include <QObject>
#include <QStringList>

#include "libsshqt.h"

/*!

    LibsshQtQuestionConsole - Console user input class for LibsshQtClient

    LibsshQtQuestionConsole implements the normal console user input questions
    needed while connecting to a SSH server. Specifically
    LibsshQtQuestionConsole handles unknownHost, needPassword and needKbiAnswers
    signals emitted by LibsshQtClient and prints the appropriate questions to
    stdout and reads user input from stdin.

    If the user types the wrong password then LibsshQtQuestionConsole re-enables
    the attempted authentication method and asks the question again.
    Specifically if LibsshQtClient emits authFailed signal and the
    failed authentication type is either UseAuthPassword or UseAuthKbi and if
    the server actually supports the attempted method, then
    LibsshQtQuestionConsole will re-enable the failed authentication method.

    If LibsshQtClient emits either allAuthsFailed or error signal then
    LibsshQtQuestionConsole prints the correct error message to stderr.

    Because LibsshQtQuestionConsole uses the standard cin.readLine() function
    this class will block the execution of the application while waiting for
    user input.

*/
class LibsshQtQuestionConsole : public QObject
{
    Q_OBJECT

public:
    explicit LibsshQtQuestionConsole(LibsshQtClient *parent);

private slots:
    void handleDebugChanged();
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
