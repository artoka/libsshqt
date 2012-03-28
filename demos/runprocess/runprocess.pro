#-------------------------------------------------
#
# Project created by QtCreator 2012-01-26T00:29:15
#
#-------------------------------------------------

QT       += core

QT       -= gui

TARGET = libsshqt-runprocess
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

include( ../../libsshqt.pri )
include( ../../libssh.pri )

SOURCES += main.cpp \
    runprocess.cpp

HEADERS += \
    runprocess.h




