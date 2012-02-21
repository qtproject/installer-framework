include(../../installerbuilder/installerbuilder.pri)

TEMPLATE = app
INCLUDEPATH += . ..
DEPENDPATH += . .. ../../installerbuilder/common

CONFIG += console
CONFIG -= app_bundle

QT += network

# Input
SOURCES += fileengineclient.cpp
