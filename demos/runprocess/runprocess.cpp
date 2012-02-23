#include <QDebug>
#include <QCoreApplication>
#include <QStringList>
#include <QUrl>

#include "runprocess.h"
#include "libsshqtquestionconsole.h"

void RunProcess::runProcess()
{
    QStringList args = qApp->arguments();

    if ( args.count() != 3) {
        qDebug() << "Usage:"
                 << qPrintable(args.at(0))
                 <<"SSH_URL COMMAND";
        qDebug() << "Example:"
                 << qPrintable(args.at(0))
                 << QString("ssh://user@hostname/")
                 << QString("ls");
        qApp->quit();
        return;
    }

    QUrl url(args.at(1));

    client = new LibsshQtClient(this);
    client->setUrl(url);
    client->useDefaultAuths();
    client->connectToHost();

    new LibsshQtQuestionConsole(client);

    process = client->runCommand(args.at(2));
}
