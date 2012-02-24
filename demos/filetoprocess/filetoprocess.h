#ifndef FILETOPROCESS_H
#define FILETOPROCESS_H

#include <QObject>
#include <QFile>

#include "libsshqt.h"

class FileToProcess : public QObject
{
    Q_OBJECT

public slots:
    void fileToProcess();
    void handleProcessError();
    void sendData();
    void endCheck();

private:
    LibsshQtClient  *client;
    LibsshQtProcess *process;
    QFile           *file;
};

#endif // FILETOPROCESS_H
