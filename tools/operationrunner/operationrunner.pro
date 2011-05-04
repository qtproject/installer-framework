TEMPLATE = app
DEPENDPATH += . .. ../../installerbuilder/common
INCLUDEPATH += . ..

DESTDIR = ../../installerbuilder/bin

CONFIG += console
CONFIG -= app_bundle

include(../../installerbuilder/libinstaller/libinstaller.pri)

# Input
SOURCES += operationrunner.cpp

LIBS = -L../../installerbuilder/lib -linstaller $$LIBS
