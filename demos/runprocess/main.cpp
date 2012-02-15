#include <QtCore/QCoreApplication>
#include <QTimer>
#include "runprocess.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    RunProcess *runprocess = new RunProcess;
    QTimer::singleShot(0, runprocess, SLOT(runProcess()));
    int ret = a.exec();
    delete runprocess;
    return ret;
}
