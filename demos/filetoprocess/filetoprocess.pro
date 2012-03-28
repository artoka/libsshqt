#-------------------------------------------------
#
# Project created by QtCreator 2012-02-09T14:13:53
#
#-------------------------------------------------

QT       += core

QT       -= gui

TARGET = libsshqt-filetoprocess
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

include( ../../libsshqt.pri )
include( ../../libssh.pri )

SOURCES += main.cpp \
    filetoprocess.cpp

HEADERS += \
    filetoprocess.h


