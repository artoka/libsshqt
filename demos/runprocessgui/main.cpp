#include <QtGui/QApplication>
#include "runprocessgui.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    RunProcessGui w;
    w.show();
    return a.exec();
}
