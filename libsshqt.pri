HEADERS += $$PWD/src/libsshqt.h
HEADERS += $$PWD/src/libsshqtquestionconsole.h
SOURCES += $$PWD/src/libsshqt.cpp
SOURCES += $$PWD/src/libsshqtquestionconsole.cpp
INCLUDEPATH += $$PWD/src

CONFIG += link_pkgconfig
PKGCONFIG += libssh
