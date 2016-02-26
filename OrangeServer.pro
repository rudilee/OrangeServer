#-------------------------------------------------
#
# Project created by QtCreator 2016-02-24T11:42:03
#
#-------------------------------------------------

include(../lib/qt-solutions/qtservice/src/qtservice.pri)

QT       += core
QT       -= gui

TARGET = orange
CONFIG   += console
CONFIG   -= app_bundle
CONFIG   += static

TEMPLATE = app

SOURCES += main.cpp \
    service.cpp

HEADERS += \
    service.h
