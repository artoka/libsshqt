
#include <QDebug>
#include <QMetaEnum>
#include <QFile>
#include <QRegExp>

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>

#include <iostream>

#include "libsshqtquestionconsole.h"

#include "libsshqtdebug.h"
#include "libsshqt.h"


LibsshQtQuestionConsole::LibsshQtQuestionConsole(LibsshQtClient *parent) :
    QObject(parent),
    debug_prefix_(LibsshQt::debugPrefix(this)),
    debug_output_(parent->isDebugEnabled()),
    client_(parent)
{
    LIBSSHQT_DEBUG("Parent client is:" << LIBSSHQT_HEXNAME(parent));

    connect(client_, SIGNAL(unknownHost()),
            this,    SLOT(handleUnknownHost()));
    connect(client_, SIGNAL(authFailed(int)),
            this,    SLOT(handleAuthFailed(int)));
    connect(client_, SIGNAL(needPassword()),
            this,    SLOT(handleNeedPassword()));
    connect(client_, SIGNAL(needKbiAnswers()),
            this,    SLOT(handleNeedKbiAnswers()));
    connect(client_, SIGNAL(error()),
            this,    SLOT(handleError()));

    if ( client_->state() == LibsshQtClient::StateUnknownHost ) {
        handleUnknownHost();

    } else if ( client_->state() == LibsshQtClient::StateAuthNeedPassword ) {
        handleNeedPassword();

    } else if ( client_->state() == LibsshQtClient::StateAuthKbiQuestions ) {
        handleNeedKbiAnswers();

    } else if ( client_->state() == LibsshQtClient::StateAuthAllFailed ) {
        handleAllAuthsFailed();
    }
}

void LibsshQtQuestionConsole::handleUnknownHost()
{
    LIBSSHQT_DEBUG("Handling unknown host");

    bool valid_input = false;
    while ( valid_input == false ) {

        std::cout << qPrintable(tr("Hostname")) << ": "
                  << qPrintable(client_->hostname()) << ":" << client_->port()
                  << std::endl
                  << qPrintable(tr("Key fingerprint")) << ": "
                  << qPrintable(client_->hostPublicKeyHash())
                  << std::endl
                  << qPrintable(client_->unknownHostMessage()) << " "
                  << qPrintable(tr("Do you want to trust this host?"))
                  << std::endl
                  << qPrintable(tr("Type Yes or No: "));
        std::cout.flush();

        QString line = readLine();
        LIBSSHQT_DEBUG("User input:" << line);

        line = line.trimmed();
        QRegExp yes_regex("^yes|y$", Qt::CaseInsensitive, QRegExp::RegExp2);
        QRegExp no_regex("^no|n$", Qt::CaseInsensitive, QRegExp::RegExp2);

        if ( yes_regex.exactMatch(line)) {
            LIBSSHQT_DEBUG("User accepted host");
            client_->markCurrentHostKnown();
            valid_input = true;

        } else if ( no_regex.exactMatch(line)) {
            LIBSSHQT_DEBUG("User refused host");
            client_->disconnectFromHost();
            valid_input = true;

        } else {
            std::cout << "Invalid input: " << qPrintable(line) << std::endl;
        }
    }
}

void LibsshQtQuestionConsole::handleNeedPassword()
{
    LIBSSHQT_DEBUG("Handling password input");

    std::cout << qPrintable(tr("Type password for")) << " "
              << qPrintable(client_->username()) << "@"
              << qPrintable(client_->hostname()) << ": ";
    std::cout.flush();

    QString password = readPassword();
    LIBSSHQT_DEBUG("Setting password");
    client_->setPassword(password);
}

void LibsshQtQuestionConsole::handleNeedKbiAnswers()
{
    LIBSSHQT_DEBUG("Handling KBI answer input");

    QStringList answers;
    foreach ( LibsshQtClient::KbiQuestion question, client_->kbiQuestions()) {

        std::cout << qPrintable(tr("Authentication question from")) << " "
                  << qPrintable(client_->hostname()) << ":" << client_->port()
                  << std::endl
                  << qPrintable(question.question);
        std::cout.flush();

        if ( question.showAnswer ) {
            answers << readLine();
        } else {
            answers << readPassword();
        }
    }

    LIBSSHQT_DEBUG("Setting KBI answers");
    client_->setKbiAnswers(answers);
}

void LibsshQtQuestionConsole::handleAuthFailed(int auth)
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

void LibsshQtQuestionConsole::handleAllAuthsFailed()
{
    std::cout << qPrintable(tr("Could not authenticate to")) << " "
              << qPrintable(client_->hostname()) << ":"
              << client_->port() << std::endl;
    std::cout.flush();

    LIBSSHQT_DEBUG("All authentication attempts have failed");
    LIBSSHQT_DEBUG("Supported auth:" << client_->supportedAuthMethods());
    LIBSSHQT_DEBUG("Failed auths:" << client_->failedAuths());
    LIBSSHQT_DEBUG("Closing connection:" << LIBSSHQT_HEXNAME(client_));
    client_->disconnectFromHost();
}

void LibsshQtQuestionConsole::handleError()
{
    std::cout << qPrintable(tr("Error")) << ": "
              << qPrintable(client_->errorMessage())
              << std::endl;
    std::cout.flush();
}

QString LibsshQtQuestionConsole::readLine()
{
    char line_array[256];
    std::cin.getline(line_array, sizeof(line_array));
    return QString::fromLocal8Bit(line_array);
}

QString LibsshQtQuestionConsole::readPassword()
{
    struct termios old_atribs, no_echo;

    // Get terminal attributes
    tcgetattr(fileno(stdin), &old_atribs);
    no_echo = old_atribs;
    no_echo.c_lflag &= ~ECHO;
    no_echo.c_lflag |= ECHONL;

    // Disable echo
    if ( tcsetattr(fileno(stdin), TCSANOW, &no_echo) != 0 ) {
        LIBSSHQT_FATAL("Failed to disable terminal echo output");
    }

    // Read line
    char line_array[256];
    std::cin.getline(line_array, sizeof(line_array));

    // Restore echo
    if ( tcsetattr(fileno(stdin), TCSANOW, &old_atribs) != 0 ) {
        LIBSSHQT_FATAL("Failed to restore terminal echo output");
    }

    return QString::fromLocal8Bit(line_array);
}



































