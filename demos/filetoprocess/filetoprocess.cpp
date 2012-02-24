
#include <QCoreApplication>
#include <QStringList>
#include <QDebug>
#include <QUrl>

#include "filetoprocess.h"
#include "libsshqtquestionconsole.h"

void FileToProcess::fileToProcess()
{
    QStringList args = qApp->arguments();

    if ( args.count() != 4) {
        qDebug() << "Usage:"
                 << qPrintable(args.at(0))
                 <<"SSH_URL FILE COMMAND";
        qDebug() << "Example:"
                 << qPrintable(args.at(0))
                 << QString("ssh://user@hostname:port/")
                 << QString("input-file")
                 << QString("cat");
        qApp->quit();
        return;
    }

    QUrl url(args.at(1));
    client = new LibsshQtClient(this);
    client->setUrl(url);
    client->useDefaultAuths();
    client->connectToHost();

    process = client->runCommand(args.at(3));

    file = new QFile(this);
    file->setFileName(args.at(2));

    if ( ! file->open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open file:" << file->fileName();
        qApp->exit(-1);
        return;
    }

    new LibsshQtQuestionConsole(client);

    connect(client, SIGNAL(allAuthsFailed()), qApp, SLOT(quit()));
    connect(client, SIGNAL(error()),          qApp, SLOT(quit()));
    connect(client, SIGNAL(closed()),         qApp, SLOT(quit()));

    connect(process, SIGNAL(opened()),             this, SLOT(sendData()));
    connect(process, SIGNAL(bytesWritten(qint64)), this, SLOT(sendData()));
    connect(process, SIGNAL(finished(int)),        this, SLOT(endCheck()));
    connect(process, SIGNAL(closed()),             qApp, SLOT(quit()));
    connect(process, SIGNAL(error()),              this, SLOT(handleProcessError()));

    connect(file, SIGNAL(readyRead()),           this, SLOT(sendData()));
    connect(file, SIGNAL(readChannelFinished()), this, SLOT(endCheck()));
}

void FileToProcess::handleProcessError()
{
    if ( ! process->isClientError()) {
        qDebug() << "Process error:"
                 << qPrintable(client->errorCodeAndMessage());
        qApp->quit();
    }
}

void FileToProcess::sendData()
{
    const int size = 1024 * 4;

    if ( process->isOpen() &&
         process->bytesToWrite() <= size &&
         file->isOpen() &&
         file->atEnd() == false ) {

        QByteArray data = file->read(size);
        if ( ! process->write(data)) {
            qDebug() << "Could not write data to process";
            qApp->exit(-1);
        } else {
            qDebug() << "Sent" << data.size() << "bytes to process";
        }
    }

    if ( file->atEnd()) {
        process->sendEof();
    }
}

void FileToProcess::endCheck()
{
    if ( ! process->isOpen()) {
        qApp->exit();
    }
}
