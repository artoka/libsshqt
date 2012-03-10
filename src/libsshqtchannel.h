#ifndef LIBSSHQTCHANNEL_H
#define LIBSSHQTCHANNEL_H

#include <QObject>
#include <QTimer>
#include <QIODevice>
#include <QSocketNotifier>
#include <libssh/libssh.h>

class LibsshQtClient;

/*!

    LibsshQtChannel

*/
class LibsshQtChannel : public QIODevice
{
    Q_OBJECT

public:
    Q_FLAGS(EofState)
    enum EofState
    {
        EofNotSent,
        EofQueued,
        EofSent
    };

    static const char *enumToString(const EofState flag);

    explicit LibsshQtChannel(bool            is_stderr,
                             LibsshQtClient *client,
                             QObject        *parent);


// QIODevice API
public:
    virtual bool atEnd() const;
    qint64 bytesAvailable() const;
    qint64 bytesToWrite() const;
    bool isSequential();
    bool canReadLine() const;
protected:
    qint64 readData(char *data, qint64 maxlen);
    qint64 writeData(const char *data, qint64 len);

// LibsshQtChannel Api
public:
    LibsshQtClient *client();

    void setWriteSize(int write_size);
    void setReadBufferSize(int buffer_size);
    int getWriteSize();
    int getBufferSize();

    void sendEof();
    EofState eofState();

    // Error handling
    bool isClientError() const;
    QString errorCodeAndMessage() const;
    QString errorMessage() const;
    int errorCode() const;

protected:
    void checkIo();
    virtual void queueCheckIo() = 0;

private slots:
    void handleDebugChanged();

protected:
    QString         debug_prefix_;
    bool            debug_output_;
    LibsshQtClient *client_;

    ssh_channel     channel_;
    bool            is_stderr_;
    EofState        eof_state_;

    int             buffer_size_;
    int             write_size_;
    QByteArray      read_buffer_;
    QByteArray      write_buffer_;
};


// Include <QDebug> before "libsshqt.h" if you want to use these operators
#ifdef QDEBUG_H

inline QDebug operator<<(QDebug dbg, const LibsshQtChannel::EofState flag)
{
    dbg << LibsshQtChannel::enumToString(flag);
    return dbg;
}

#endif

#endif // LIBSSHQTCHANNEL_H


























