
#include <QtCore/QCoreApplication>
#include <QTimer>

#include "filetoprocess.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    FileToProcess *filetoprocess = new FileToProcess;
    QTimer::singleShot(0, filetoprocess, SLOT(fileToProcess()));
    int ret = a.exec();
    delete filetoprocess;
    return ret;
}
