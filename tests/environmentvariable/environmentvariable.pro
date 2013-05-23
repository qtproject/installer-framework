TEMPLATE = app
INCLUDEPATH += . ..
TARGET = environmentvariable

include(../../installerfw.pri)

QT -= gui
QT += testlib

CONFIG += console
DESTDIR = $$IFW_APP_PATH

HEADERS = environmentvariabletest.h 
SOURCES = environmentvariabletest.cpp

macx:include(../../no_app_bundle.pri)
