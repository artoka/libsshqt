
#include <QTextDocument>
#include <QSettings>

#include "runprocessgui.h"
#include "ui_runprocessgui.h"

RunProcessGui::RunProcessGui(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::runprocessgui),
    client(new LibsshQtClient(this)),
    process(0),
    gui(new LibsshQtGui(this))
{
    ui->setupUi(this);
    client->setDebug(true);
    gui->setClient(client);

    qApp->setOrganizationName("libsshqt");
    qApp->setApplicationName("runprocessqui");

    QSettings settings;
    ui->username_edit->setText(settings.value("username").toString());
    ui->hostname_edit->setText(settings.value("hostname").toString());
    ui->port_edit->setText(settings.value("port").toString());
    ui->command_edit->setText(settings.value("command").toString());
}

RunProcessGui::~RunProcessGui()
{
    delete ui;
}

void RunProcessGui::on_run_button_clicked()
{
    QSettings settings;
    settings.setValue("username", ui->username_edit->text());
    settings.setValue("hostname", ui->hostname_edit->text());
    settings.setValue("port",     ui->port_edit->text());
    settings.setValue("command",  ui->command_edit->text());

    if ( process ) {
        process->closeChannel();
        process->deleteLater();
        process = 0;
    }

    client->setUsername(ui->username_edit->text());
    client->setHostname(ui->hostname_edit->text());
    client->setPort(ui->port_edit->text().toInt());
    client->connectToHost();

    process = client->runCommand(ui->command_edit->text());
    process->setStdoutBehaviour(LibsshQtProcess::OutputManual);
    process->setStderrBehaviour(LibsshQtProcess::OutputManual);

    connect(process, SIGNAL(opened()),      this, SLOT(handleProcessOpened()));
    connect(process, SIGNAL(closed()),      this, SLOT(handleProcessClosed()));
    connect(process, SIGNAL(error()),       this, SLOT(handleProcessError()));
    connect(process, SIGNAL(finished(int)), this, SLOT(handleProcessFinished(int)));

    connect(process,           SIGNAL(readyRead()), this, SLOT(readProcessStdout()));
    connect(process->stderr(), SIGNAL(readyRead()), this, SLOT(readProcessStderr()));
}

void RunProcessGui::handleProcessOpened()
{
    statusBar()->showMessage("Running: " + process->command());
}

void RunProcessGui::handleProcessClosed()
{
    statusBar()->showMessage("Process stopped");
}

void RunProcessGui::handleProcessError()
{
    statusBar()->showMessage("Error: " + process->errorCodeAndMessage());
}

void RunProcessGui::handleProcessFinished(int exit_code)
{
    statusBar()->showMessage("Process finished with exit code " + exit_code);
}

void RunProcessGui::readProcessStdout()
{
    while ( process->canReadLine()) {
        QString line = process->readLine();
        ui->output_edit->appendPlainText(line);
    }
}

void RunProcessGui::readProcessStderr()
{
    while ( process->canReadLine()) {
        QString line = process->readLine();
        ui->output_edit->appendHtml(
                    QString("<font color=red>%1</font>")
                        .arg(Qt::escape(line)));
    }
}































