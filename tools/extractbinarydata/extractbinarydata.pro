include(../../installerbuilder/installerbuilder.pri)

TEMPLATE = app
INCLUDEPATH += . ..
DEPENDPATH += . .. ../../installerbuilder/common

CONFIG += console
CONFIG -= app_bundle

# Input
SOURCES += main.cpp

HEADERS +=
