#-------------------------------------------------
#
# Project created by QtCreator 2012-03-12T13:26:00
#
#-------------------------------------------------

QT       += testlib

QT       -= gui

TARGET = libsshqt-test
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += test.cpp
DEFINES += SRCDIR=\\\"$$PWD/\\\"

include( ../libsshqt.pri )
