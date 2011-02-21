TEMPLATE = app
TARGET = operationrunner
DEPENDPATH += . .. ../common
INCLUDEPATH += . ..

DESTDIR = ../bin

CONFIG += console
CONFIG -= app_bundle

QT += xml

include(../libinstaller/libinstaller.pri)

# Input
SOURCES += operationrunner.cpp

LIBS = -L../../installerbuilder/lib -linstaller $$LIBS
