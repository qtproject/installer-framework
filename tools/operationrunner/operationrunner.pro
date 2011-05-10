TEMPLATE = app
TARGET = operationrunner
DEPENDPATH += . .. ../../installerbuilder/common
INCLUDEPATH += . ..

DESTDIR = ../../installerbuilder/bin

CONFIG += console
CONFIG -= app_bundle

QT += xml

include(../../installerbuilder/libinstaller/libinstaller.pri)

# Input
SOURCES += operationrunner.cpp \
    fakeinstaller.cpp

HEADERS += \
    fakeinstaller.h

LIBS = -L../../installerbuilder/lib -linstaller $$LIBS
