#ifndef RUNPROCESS_H
#define RUNPROCESS_H

#include <QObject>
#include "libsshqtclient.h"
#include "libsshqtprocess.h"

class RunProcess : public QObject
{
    Q_OBJECT

public slots:
    void runProcess();
    void handleProcessError();

private:
    LibsshQtClient  *client;
    LibsshQtProcess *process;
};

#endif // RUNPROCESS_H
