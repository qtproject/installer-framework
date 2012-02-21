include(../../installerbuilder/installerbuilder.pri)

DEPENDPATH += ../../installerbuilder/libinstaller ../../installerbuilder/common
INCLUDEPATH += ../../installerbuilder/libinstaller ../../installerbuilder/common

TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
TARGET = downloadspeed

QT -= gui
QT += core network

SOURCES += main.cpp
