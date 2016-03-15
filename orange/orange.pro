#-------------------------------------------------
#
# Project created by QtCreator 2016-03-03T12:26:46
#
#-------------------------------------------------

include(../../lib/qt-solutions/qtservice/src/qtservice.pri)

QT       += core network sql
QT       -= gui

TARGET = orange

CONFIG   += console
CONFIG   -= app_bundle
CONFIG   += static

TEMPLATE = app

SOURCES += main.cpp \
    service.cpp \
    worker.cpp \
    client.cpp

HEADERS += \
    service.h \
    worker.h \
    client.h \
    common.h \
    terminal.h
