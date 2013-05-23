TEMPLATE = app
INCLUDEPATH += . ..
TARGET = fileengineclient

include(../../installerfw.pri)

QT -= gui
QT += network

CONFIG += console
DESTDIR = $$IFW_APP_PATH

SOURCES += fileengineclient.cpp

macx:include(../../no_app_bundle.pri)
