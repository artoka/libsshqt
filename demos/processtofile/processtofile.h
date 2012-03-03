#ifndef PROCESSTOFILE_H
#define PROCESSTOFILE_H

#include <QObject>
#include <QFile>

#include "libsshqtclient.h"
#include "libsshqtprocess.h"

class ProcessToFile : public QObject
{
    Q_OBJECT

public slots:
    void processToFile();
    void handleProcessError();
    void copydata();
    void endcheck();

private:
    LibsshQtClient  *client;
    LibsshQtProcess *process;
    QFile           *file;
};

#endif // PROCESSTOFILE_H
