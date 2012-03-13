TEMPLATE = app
TARGET = extractarchiveoperationtest

DESTDIR = bin

CONFIG -= app_bundle

QT += testlib script
QT -= gui

INCLUDEPATH += . .. ../../installerbuilder/libinstaller
DEPENDPATH += . .. ../../installerbuilder/libinstaller

include(../../installerbuilder/libinstaller/libinstaller.pri)

SOURCES = extractarchiveoperationtest.cpp
HEADERS = extractarchiveoperationtest.h 

LIBS = -L../../installerbuilder/lib -linstaller $$LIBS
