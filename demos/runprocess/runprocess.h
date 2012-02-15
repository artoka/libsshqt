#ifndef RUNPROCESS_H
#define RUNPROCESS_H

#include <QObject>
#include "libsshqt.h"

class RunProcess : public QObject
{
    Q_OBJECT

public slots:
    void runProcess();

private:
    LibsshQtClient  *client;
    LibsshQtProcess *process;
};

#endif // RUNPROCESS_H
