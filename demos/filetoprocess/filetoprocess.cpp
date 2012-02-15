
#include <QCoreApplication>
#include <QStringList>
#include <QDebug>
#include <QUrl>

#include "filetoprocess.h"

void FileToProcess::fileToProcess()
{
    QStringList args = qApp->arguments();
    if ( args.count() < 3 ) {
        qDebug() << "Usage:" << qPrintable(args.first())
                 << "SSH_URL PASSWORD LOCAL_FILE REMOTE_COMMAND";
        qApp->exit(-1);
        return;
    }

    QUrl url(args.at(1));
    client_ = new LibsshQtClient(this);
    client_->setUrl(url);
    client_->usePasswordAuth(true, args.at(2));
    client_->connectToHost();

    process_ = client_->runCommand(args.at(4));

    connect(process_, SIGNAL(opened()),             this, SLOT(sendData()));
    connect(process_, SIGNAL(bytesWritten(qint64)), this, SLOT(sendData()));
    connect(process_, SIGNAL(finished(int)),        this, SLOT(endCheck()));


    file_ = new QFile(this);
    file_->setFileName(args.at(3));
    connect(file_, SIGNAL(readyRead()),           this, SLOT(sendData()));
    connect(file_, SIGNAL(readChannelFinished()), this, SLOT(endCheck()));
    //connect(file_, SIGNAL(bytesWritten(qint64)), this, SLOT(endcheck(qint64)));

    if ( ! file_->open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open file:" << file_->fileName();
        qApp->exit(-1);
        return;
    }
}

void FileToProcess::sendData()
{
    const int size = 1024 * 4;

    if ( process_->isOpen() &&
         process_->bytesToWrite() <= size &&
         file_->isOpen() &&
         file_->atEnd() == false ) {

        QByteArray data = file_->read(size);
        if ( ! process_->write(data)) {
            qDebug() << "Could not write data to process";
            qApp->exit(-1);
        } else {
            qDebug() << "Sent" << data.size() << "bytes to process";
        }
    }

    if ( file_->atEnd()) {
        process_->sendEof();
    }
}

/*
void FileToProcess::endCheck(qint64)
{
    endCheck();
}
*/

void FileToProcess::endCheck()
{
    if ( ! process_->isOpen()) {
        qApp->exit();
    }
}
