#include <QDebug>
#include <QCoreApplication>
#include <QStringList>
#include <QUrl>

#include "runprocess.h"

void RunProcess::runProcess()
{
    QStringList args = qApp->arguments();

    if ( args.count() != 4 ) {
        qDebug() << "Usage:"
                 << qPrintable(args.at(0))
                 <<"SSH_URL PASSWORD COMMAND";
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
    client->usePasswordAuth(true, args.at(2));
    client->connectToHost();

    process = client->runCommand(args.at(3));
}
