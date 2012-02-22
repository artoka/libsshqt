#ifndef RUNPROCESSGUI_H
#define RUNPROCESSGUI_H

#include <QMainWindow>

#include "libsshqt.h"
#include "libsshqtgui.h"

namespace Ui {
    class runprocessgui;
}

class RunProcessGui : public QMainWindow
{
    Q_OBJECT

public:
    explicit RunProcessGui(QWidget *parent = 0);
    ~RunProcessGui();

    void readSettings();
    void saveSettings();

private slots:
    void on_run_button_clicked();

    void handleProcessOpened();
    void handleProcessClosed();
    void handleProcessError();
    void handleProcessFinished(int exit_code);

    void readProcessStdout();
    void readProcessStderr();

private:
    Ui::runprocessgui       *ui;
    LibsshQtClient          *client;
    LibsshQtProcess         *process;
    LibsshQtQuestionDialog  *gui;
};

#endif // RUNPROCESSGUI_H
