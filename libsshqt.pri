HEADERS += $$PWD/src/libsshqt.h
SOURCES += $$PWD/src/libsshqt.cpp
INCLUDEPATH += $$PWD/src

CONFIG += link_pkgconfig
PKGCONFIG += libssh
