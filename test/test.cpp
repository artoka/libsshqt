
#include <QDebug>
#include <QtCore/QString>
#include <QtTest/QtTest>
#include <QtConcurrentRun>

#include "libsshqtclient.h"
#include "libsshqtprocess.h"



//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// TestCaseBase
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

class TestCaseOpts
{
public:
    QEventLoop  loop;
    QUrl        url;
    QString     password;
};

class TestCaseBase : public QObject
{
    Q_OBJECT

public:
    TestCaseBase(TestCaseOpts *opts);
    void testSuccess();
    void testFailed();

public slots:
    void handleError();

public:
    TestCaseOpts   * const opts;
    LibsshQtClient * const client;
};

TestCaseBase::TestCaseBase(TestCaseOpts *opts) :
    opts(opts),
    client(new LibsshQtClient(this))
{
    //client->setDebug(true);

    connect(client, SIGNAL(error()),
            this,   SLOT(handleError()));
    connect(client, SIGNAL(closed()),
            this,   SLOT(handleError()));
    connect(client, SIGNAL(allAuthsFailed()),
            this,   SLOT(handleError()));

    client->setUrl(opts->url);
    client->usePasswordAuth(true);
    client->setPassword(opts->password);
    client->connectToHost();
}

void TestCaseBase::testSuccess()
{
    opts->loop.exit(0);
    client->disconnect(this);
}

void TestCaseBase::testFailed()
{
    opts->loop.exit(-1);
    client->disconnect(this);
}

void TestCaseBase::handleError()
{
    testFailed();
}



//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// TestConnect
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

class TestCaseConnect : public TestCaseBase
{
    Q_OBJECT

public:
    TestCaseConnect(TestCaseOpts *opts);

public slots:
    void connectTest();
};

TestCaseConnect::TestCaseConnect(TestCaseOpts *opts) :
    TestCaseBase(opts)
{
    connect(client, SIGNAL(opened()),
            this,   SLOT(connectTest()));
}

void TestCaseConnect::connectTest()
{
    testSuccess();
}



//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// TestCaseReadline
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

class TestCaseReadline : public TestCaseBase
{
    Q_OBJECT

public:
    TestCaseReadline(TestCaseOpts *opts);

public slots:
    void readLine();
    void finished();

public:
    LibsshQtProcess *process;
    QIODevice       *readdev;
    QStringList      expected;
    int              line_pos;
};

TestCaseReadline::TestCaseReadline(TestCaseOpts *opts) :
    TestCaseBase(opts),
    process(0),
    readdev(0),
    line_pos(0)
{
    expected << "first" << "second" << "last";
}

void TestCaseReadline::readLine()
{
    while ( readdev->canReadLine()) {
        QString line = QString(readdev->readLine());
        if ( line.endsWith('\n')) {
            line.remove(line.length() - 1, 1);
        }

        if ( line == expected.at(line_pos)) {
            qDebug() << "Readline:" << line;
            line_pos++;

        } else {
            qDebug() << "Invalid line:" << line;
            testFailed();
            return;
        }
    }
}

void TestCaseReadline::finished()
{
    if ( line_pos == expected.count()) {
        testSuccess();
    } else {
        qDebug() << "Invalid number of lines read:" << line_pos;
        testFailed();
    }
}



//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// TestCaseReadlineStdout
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

class TestCaseReadlineStdout : public TestCaseReadline
{
    Q_OBJECT

public:
    TestCaseReadlineStdout(TestCaseOpts *opts);
};

TestCaseReadlineStdout::TestCaseReadlineStdout(TestCaseOpts *opts) :
    TestCaseReadline(opts)
{
    QString command = QString("echo -en %1").arg(expected.join("\\\\n"));
    qDebug("Command: %s", qPrintable(command));

    process = client->runCommand(command);
    process->setStdoutBehaviour(LibsshQtProcess::OutputManual);
    readdev = process;

    connect(process, SIGNAL(finished(int)),
            this,    SLOT(finished()));
    connect(process, SIGNAL(readyRead()),
            this,    SLOT(readLine()));
}



//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// TestCaseReadlineStderr
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

class TestCaseReadlineStderr : public TestCaseReadline
{
    Q_OBJECT

public:
    TestCaseReadlineStderr(TestCaseOpts *opts);
};

TestCaseReadlineStderr::TestCaseReadlineStderr(TestCaseOpts *opts) :
    TestCaseReadline(opts)
{
    QString command = QString("echo -en %1 1>&2").arg(expected.join("\\\\n"));
    qDebug("Command: %s", qPrintable(command));

    process = client->runCommand(command);
    process->setStderrBehaviour(LibsshQtProcess::OutputManual);
    readdev = process->stderr();

    connect(process, SIGNAL(finished(int)),
            this,    SLOT(finished()));
    connect(readdev, SIGNAL(readyRead()),
            this,    SLOT(readLine()));
}



//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// TestCaseIO
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

class TestCaseIO : public TestCaseBase
{
    Q_OBJECT

public:
    TestCaseIO(TestCaseOpts *opts);
    quint16 posToValue(int pos);

public slots:
    void writeData();
    void readData();

public:
    QIODevice *writedev;
    QIODevice *readdev;

    int error_count;
    int max_uint16;
    int write_pos;
    int read_pos;
};

TestCaseIO::TestCaseIO(TestCaseOpts *opts) :
    TestCaseBase(opts),
    writedev(0),
    readdev(0),
    error_count(0),
    max_uint16(0xFFFF),
    write_pos(0),
    read_pos(0)
{
}

quint16 TestCaseIO::posToValue(int pos)
{
    int val = pos % ( max_uint16 + 1 );
    //qDebug() << "Max:" << max << " Pos:" << pos << " Val:" << val;
    return val;
}

void TestCaseIO::writeData()
{
    if ( writedev->bytesToWrite() == 0 && write_pos <= max_uint16 + 1 ) {
        quint16 val = posToValue(write_pos);
        QByteArray data(reinterpret_cast< const char* >( &val ), sizeof(val));
        writedev->write(data);
        write_pos++;
    }
}

void TestCaseIO::readData()
{
    while ( readdev->bytesAvailable() >= 2 && read_pos <= max_uint16 + 1 ) {

        quint16 read_val = 0;
        readdev->read(reinterpret_cast< char* >( &read_val ), sizeof(quint16));

        quint16 correct_val = posToValue(read_pos);
        if ( read_val != correct_val ) {
            error_count++;
            qDebug() << "Position:"  << read_pos
                     << " Read:"     << read_val
                     << " Expected:" << correct_val
                     << " Diff:"     << read_val - correct_val;
        }
        read_pos++;
    }

    if ( read_pos > max_uint16 ) {
        qDebug() << "Found" << error_count << "errors";
        if ( error_count == 0 ) {
            testSuccess();
        } else {
            testFailed();
        }
    }
}



//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// TestCaseIOStdout
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

class TestCaseIOStdout : public TestCaseIO
{
    Q_OBJECT

public:
    TestCaseIOStdout(TestCaseOpts *opts);
};

TestCaseIOStdout::TestCaseIOStdout(TestCaseOpts *opts) :
    TestCaseIO(opts)
{
    LibsshQtProcess *process = client->runCommand("cat");
    process->setStdoutBehaviour(LibsshQtProcess::OutputManual);

    readdev  = process;
    writedev = process;

    connect(process, SIGNAL(opened()),
            this,    SLOT(writeData()));
    connect(process, SIGNAL(bytesWritten(qint64)),
            this,    SLOT(writeData()));
    connect(process, SIGNAL(readyRead()),
            this,    SLOT(readData()));
}



//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// TestCaseIOStderr
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

class TestCaseIOStderr : public TestCaseIO
{
    Q_OBJECT

public:
    TestCaseIOStderr(TestCaseOpts *opts);
};

TestCaseIOStderr::TestCaseIOStderr(TestCaseOpts *opts) :
    TestCaseIO(opts)
{
    LibsshQtProcess *process = client->runCommand("cat 1>&2");
    process->setStderrBehaviour(LibsshQtProcess::OutputManual);

    readdev  = process->stderr();
    writedev = process;

    connect(process, SIGNAL(opened()),
            this,    SLOT(writeData()));
    connect(process, SIGNAL(bytesWritten(qint64)),
            this,    SLOT(writeData()));
    connect(readdev, SIGNAL(readyRead()),
            this,    SLOT(readData()));
}



//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// Test
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

class Test : public QObject
{
    Q_OBJECT

public:
    Test();

private Q_SLOTS:
    void testConnect();
    void testReadlineStdin();
    void testReadlineStderr();
    void testIoStdout();
    void testIoStderr();

private:
    TestCaseOpts opts;
};

Test::Test()
{
    const char conf_file[] = "./libsshqt-test-config";
    const QString conf_help = QString(
                "Please create configuration file:\n"
                "\n"
                "    %1\n"
                "\n"
                "with the contents:\n"
                "\n"
                "    ssh://user@hostname:port/\n"
                "    password\n")
                .arg(conf_file);

    QFile file;
    file.setFileName(conf_file);
    if ( ! file.open(QIODevice::ReadOnly)) {
        qFatal("\nError: Could not open test configuration file: %s\n\n%s",
               conf_file, qPrintable(conf_help));
    }

    QStringList config = QString(file.readAll()).split('\n');
    if ( ! config.length() == 2 ) {
        qFatal("\nError: Could not read SSH url and password from: %s\n\n%s",
               conf_file, qPrintable(conf_help));
    }

    opts.url = config.at(0);
    opts.password = config.at(1);
}

void Test::testConnect()
{
    TestCaseConnect testcase(&opts);
    QVERIFY2(opts.loop.exec() == 0, "Could not connect to the SSH server");
}

void Test::testReadlineStdin()
{
    TestCaseReadlineStdout testcase(&opts);
    QVERIFY2(opts.loop.exec() == 0, "Could not read lines correctly from STDOUT");
}

void Test::testReadlineStderr()
{
    TestCaseReadlineStderr testcase(&opts);
    QVERIFY2(opts.loop.exec() == 0, "Could not read lines correctly from STDERR");
}

void Test::testIoStdout()
{
    TestCaseIOStdout testcase(&opts);
    QVERIFY2(opts.loop.exec() == 0, "Data corruption in STDOUT stream");
}

void Test::testIoStderr()
{
    TestCaseIOStderr testcase(&opts);
    QVERIFY2(opts.loop.exec() == 0, "Data corruption in STDERR stream");
}

QTEST_MAIN(Test);

#include "test.moc"





























































