#include <QCoreApplication>
#include <QStringList>
#include <QDebug>
#include <QUrl>

#include "processtofile.h"

void ProcessToFile::processToFile()
{
    QStringList args = qApp->arguments();
    if ( args.count() < 3 ) {
        qDebug() << "Usage:" << qPrintable(args.first())
                 << "SSH_URL PASSWORD COMMAND OUTPUT_FILE";
        qApp->exit(-1);
        return;
    }

    QUrl url(args.at(1));
    client_ = new LibsshQtClient(this);
    client_->setUrl(url);
    client_->usePasswordAuth(true, args.at(2));
    client_->connectToHost();

    process_ = client_->runCommand(args.at(3));
    process_->setStdoutBehaviour(LibsshQtProcess::OutputManual);
    connect(process_, SIGNAL(readyRead()), this, SLOT(copydata()));
    connect(process_, SIGNAL(closed()),    this, SLOT(endcheck()));

    file_ = new QFile(this);
    file_->setFileName(args.at(4));
    connect(file_, SIGNAL(bytesWritten(qint64)), this, SLOT(endcheck(qint64)));

    if ( ! file_->open(QIODevice::ReadWrite | QIODevice::Truncate)) {
        qDebug() << "Could not open file:" << file_->fileName();
        qApp->exit(-1);
        return;
    }
}

void ProcessToFile::copydata()
{
    QByteArray data = process_->read(process_->bytesAvailable());
    if ( file_->write(data) != data.size()) {
        qDebug() << "Data write error";
        qApp->exit(-1);
    } else {
        qDebug() << "Wrote" << data.size() << "bytes to file";
    }
}

void ProcessToFile::endcheck(qint64)
{
    endcheck();
}

void ProcessToFile::endcheck()
{
    if ( file_->bytesToWrite() == 0 &&
         process_->bytesAvailable() == 0 ) {
        file_->close();
        qApp->exit();
    }
}
