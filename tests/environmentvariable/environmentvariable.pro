TEMPLATE = app
TARGET = tst_environmentvariable

DESTDIR = bin

CONFIG -= app_bundle

QT += testlib script
QT -= gui

INCLUDEPATH += ../../installerbuilder/libinstaller ..
DEPENDPATH += ../../installerbuilder/libinstaller ../../installerbuilder/common

include(../../installerbuilder/libinstaller/libinstaller.pri)

SOURCES = environmentvariabletest.cpp
HEADERS = environmentvariabletest.h 

LIBS = -L../../installerbuilder/lib -linstaller $$LIBS
win32:LIBS += ole32.lib oleaut32.lib user32.lib
win32:OBJECTS_DIR = .obj

