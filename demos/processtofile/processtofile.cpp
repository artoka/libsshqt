#include <QCoreApplication>
#include <QStringList>
#include <QDebug>
#include <QUrl>

#include "processtofile.h"
#include "libsshqtquestionconsole.h"

void ProcessToFile::processToFile()
{
    QStringList args = qApp->arguments();

    if ( args.count() != 4) {
        qDebug() << "Usage:"
                 << qPrintable(args.at(0))
                 <<"SSH_URL COMMAND FILE";
        qDebug() << "Example:"
                 << qPrintable(args.at(0))
                 << QString("ssh://user@hostname:port/")
                 << QString("ls")
                 << QString("ls-output");
        qApp->quit();
        return;
    }

    QUrl url(args.at(1));
    client = new LibsshQtClient(this);
    client->setUrl(url);
    client->useDefaultAuths();
    client->connectToHost();

    process = client->runCommand(args.at(2));
    process->setStdoutBehaviour(LibsshQtProcess::OutputManual);

    file = new QFile(this);
    file->setFileName(args.at(3));
    connect(file, SIGNAL(bytesWritten(qint64)), this, SLOT(endcheck()));

    if ( ! file->open(QIODevice::ReadWrite | QIODevice::Truncate)) {
        qDebug() << "Could not open file:" << file->fileName();
        qApp->exit(-1);
        return;
    }

    new LibsshQtQuestionConsole(client);

    connect(client, SIGNAL(allAuthsFailed()), qApp, SLOT(quit()));
    connect(client, SIGNAL(error()),          qApp, SLOT(quit()));
    connect(client, SIGNAL(closed()),         qApp, SLOT(quit()));

    connect(process, SIGNAL(readyRead()),     this, SLOT(copydata()));
    connect(process, SIGNAL(closed()),        this, SLOT(endcheck()));
    connect(process, SIGNAL(error()),         this, SLOT(handleProcessError()));
}

void ProcessToFile::handleProcessError()
{
    if ( ! process->isClientError()) {
        qDebug() << "Process error:"
                 << qPrintable(client->errorCodeAndMessage());
        qApp->quit();
    }
}

void ProcessToFile::copydata()
{
    QByteArray data = process->read(process->bytesAvailable());
    if ( file->write(data) != data.size()) {
        qDebug() << "Data write error";
        qApp->exit(-1);
    } else {
        qDebug() << "Wrote" << data.size() << "bytes to file";
    }
}

void ProcessToFile::endcheck()
{
    if ( file->bytesToWrite() == 0 &&
         process->bytesAvailable() == 0 ) {
        file->close();
        qApp->exit();
    }
}
