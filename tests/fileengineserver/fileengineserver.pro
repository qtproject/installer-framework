TEMPLATE = app
INCLUDEPATH += . ..
TARGET = fileengineserver

include(../../installerfw.pri)

QT -= gui

CONFIG += console
DESTDIR = $$IFW_APP_PATH

SOURCES += fileengineserver.cpp

macx:include(../../no_app_bundle.pri)
