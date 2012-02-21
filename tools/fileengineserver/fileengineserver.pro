include(../../installerbuilder/installerbuilder.pri)

TEMPLATE = app
DEPENDPATH += . .. ../../installerbuilder/common
INCLUDEPATH += . ..

CONFIG += console
CONFIG -= app_bundle

# Input
SOURCES += fileengineserver.cpp
