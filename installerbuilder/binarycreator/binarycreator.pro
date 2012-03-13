TEMPLATE = app
TARGET = binarycreator

DEPENDPATH += . .. rcc ../common
INCLUDEPATH += . .. rcc ../common

DESTDIR = ../bin

CONFIG += console
CONFIG -= app_bundle

include(../libinstaller/libinstaller.pri)

RESOURCES += binarycreator.qrc

# Input
SOURCES = binarycreator.cpp \
          rcc.cpp \
          rccmain.cpp \
          repositorygen.cpp
HEADERS = rcc.h
