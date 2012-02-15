
#include <QtCore/QCoreApplication>
#include <QTimer>

#include "processtofile.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    ProcessToFile *processtofile = new ProcessToFile;
    QTimer::singleShot(0, processtofile, SLOT(processToFile()));
    int ret = a.exec();
    delete processtofile;
    return ret;
}
