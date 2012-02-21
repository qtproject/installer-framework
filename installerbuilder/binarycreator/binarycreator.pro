include(../installerbuilder.pri)

TEMPLATE = app
TARGET = binarycreator

DEPENDPATH += . ..
INCLUDEPATH += . .. rcc

CONFIG += console
CONFIG -= app_bundle

RESOURCES += binarycreator.qrc

# Input
SOURCES = binarycreator.cpp \
          rcc/rcc.cpp \
          rcc/rccmain.cpp \
          ../common/repositorygen.cpp 

HEADERS = rcc/rcc.h
