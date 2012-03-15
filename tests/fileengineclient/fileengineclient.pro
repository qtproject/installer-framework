TEMPLATE = app
DEPENDPATH += . ..
INCLUDEPATH += . ..
TARGET = fileengineclient

include(../../installerfw.pri)

QT -= gui
QT += network
LIBS += -linstaller

CONFIG += console
CONFIG -= app_bundle
DESTDIR = $$IFW_APP_PATH

SOURCES += fileengineclient.cpp
