#-------------------------------------------------
#
# Project created by QtCreator 2012-02-09T12:30:54
#
#-------------------------------------------------

QT       += core

QT       -= gui

TARGET = libsshqt-processtofile
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

include( ../../libsshqt.pri )
include( ../../libssh.pri )

SOURCES += main.cpp \
    processtofile.cpp

HEADERS += \
    processtofile.h




