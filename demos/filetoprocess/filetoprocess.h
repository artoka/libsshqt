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
    void sendData();
    //void endCheck(qint64);
    void endCheck();

private:
    LibsshQtClient  *client_;
    LibsshQtProcess *process_;
    QFile           *file_;
};

#endif // FILETOPROCESS_H
