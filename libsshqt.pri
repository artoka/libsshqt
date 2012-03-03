HEADERS += $$PWD/src/libsshqtchannel.h
HEADERS += $$PWD/src/libsshqtclient.h
HEADERS += $$PWD/src/libsshqtprocess.h
HEADERS += $$PWD/src/libsshqtquestionconsole.h

SOURCES += $$PWD/src/libsshqtchannel.cpp
SOURCES += $$PWD/src/libsshqtclient.cpp
SOURCES += $$PWD/src/libsshqtprocess.cpp
SOURCES += $$PWD/src/libsshqtquestionconsole.cpp

INCLUDEPATH += $$PWD/src

CONFIG += link_pkgconfig
PKGCONFIG += libssh
