TEMPLATE = app
TARGET = tst_environmentvariable

DESTDIR = bin

CONFIG -= app_bundle

QT += testlib script
QT -= gui

INCLUDEPATH += ../../libinstaller ..
DEPENDPATH += ../../libinstaller ../../common

include(../../libinstaller/libinstaller.pri)

SOURCES = environmentvariabletest.cpp
HEADERS = environmentvariabletest.h 

win32:LIBS += ole32.lib oleaut32.lib user32.lib
win32:OBJECTS_DIR = .obj
