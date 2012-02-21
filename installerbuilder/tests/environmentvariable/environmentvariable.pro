include(../../installerbuilder.pri)

TEMPLATE = app
TARGET = tst_environmentvariable

CONFIG -= app_bundle

QT += testlib script
QT -= gui

INCLUDEPATH += ../../libinstaller ..
DEPENDPATH += ../../libinstaller ../../common

SOURCES = environmentvariabletest.cpp
HEADERS = environmentvariabletest.h 
