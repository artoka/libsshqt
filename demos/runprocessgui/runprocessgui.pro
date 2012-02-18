#-------------------------------------------------
#
# Project created by QtCreator 2012-02-18T12:43:07
#
#-------------------------------------------------

QT       += core gui

TARGET = libsshqt-runprocessgui
TEMPLATE = app

#include(../../libsshqt.pri);
include(../../libsshqtgui.pri)

SOURCES += main.cpp\
        runprocessgui.cpp

HEADERS  += runprocessgui.h

FORMS    += runprocessgui.ui
