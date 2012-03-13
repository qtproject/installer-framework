TEMPLATE = app
TARGET = tst_environmentvariable

DESTDIR = bin

CONFIG -= app_bundle

QT += testlib script
QT -= gui

INCLUDEPATH += . .. ../../installerbuilder/libinstaller
DEPENDPATH += . .. ../../installerbuilder/libinstaller

include(../../installerbuilder/libinstaller/libinstaller.pri)

SOURCES = environmentvariabletest.cpp
HEADERS = environmentvariabletest.h 

LIBS = -L../../installerbuilder/lib -linstaller $$LIBS

