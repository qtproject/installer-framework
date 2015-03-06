TEMPLATE = app
INCLUDEPATH += . ..
TARGET = downloadspeed

include(../../installerfw.pri)

QT -= gui
QT += network

CONFIG += console

SOURCES += main.cpp

macx:include(../../no_app_bundle.pri)
