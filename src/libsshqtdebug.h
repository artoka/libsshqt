#ifndef LIBSSHQTDEBUG_H
#define LIBSSHQTDEBUG_H

#include <QObject>
#include <QDebug>

#define LIBSSHQT_DEBUG(_message_) \
    if ( debug_output_ ) { \
        qDebug() << qPrintable(debug_prefix_) << _message_; \
    }

#define LIBSSHQT_CRITICAL(_message_) \
    qCritical() << qPrintable(debug_prefix_) << _message_;

#define LIBSSHQT_FATAL(_message_) \
    qFatal("%s Fatal error: %s", qPrintable(debug_prefix_), _message_);

#define LIBSSHQT_HEXNAME(_object_) \
    qPrintable(LibsshQt::hexAndName(_object_))

namespace LibsshQt
{
    inline QString hexAndName(QObject *object)
    {
        quint64 val    = reinterpret_cast<quint64>(object);
        QString hex    = QString("%1").arg(val, 0, 16);
        QString name   = object->metaObject()->className();
        return hex.toUpper() + "-" + name;
    }

    inline QString debugPrefix(QObject *object)
    {
        return hexAndName(object) + ":";
    }
}

#endif // LIBSSHQTDEBUG_H
