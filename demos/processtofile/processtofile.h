#ifndef PROCESSTOFILE_H
#define PROCESSTOFILE_H

#include <QObject>
#include <QFile>

#include "libsshqt.h"

class ProcessToFile : public QObject
{
    Q_OBJECT

public slots:
    void processToFile();
    void copydata();
    void endcheck(qint64);
    void endcheck();

private:
    LibsshQtClient  *client_;
    LibsshQtProcess *process_;
    QFile           *file_;
};

#endif // PROCESSTOFILE_H
