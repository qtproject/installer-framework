TEMPLATE = app
INCLUDEPATH += . ..
TARGET = fileengineclient

include(../../installerfw.pri)

QT -= gui
QT += network

CONFIG += console
CONFIG -= app_bundle
DESTDIR = $$IFW_APP_PATH

SOURCES += fileengineclient.cpp
