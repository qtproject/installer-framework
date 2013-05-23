TEMPLATE = app
INCLUDEPATH += . ..
TARGET = downloadspeed

include(../../installerfw.pri)

QT -= gui
QT += network

CONFIG += console

DESTDIR = $$IFW_APP_PATH

SOURCES += main.cpp

macx:include(../../no_app_bundle.pri)
