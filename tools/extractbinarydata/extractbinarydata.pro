TEMPLATE = app
DEPENDPATH += . ..
INCLUDEPATH += . ..

DESTDIR = ../../installerbuilder/bin

CONFIG += console
CONFIG -= app_bundle

include(../../installerbuilder/libinstaller/libinstaller.pri)

# Input
SOURCES += main.cpp

HEADERS +=

LIBS = -L../../installerbuilder/lib -linstaller $$LIBS
