TEMPLATE = app
DEPENDPATH += . ..
INCLUDEPATH += . ..
TARGET = maddehelper

include(../../installerfw.pri)

QT -= gui

CONFIG += console
CONFIG -= app_bundle
DESTDIR = $$IFW_APP_PATH

SOURCES += main.cpp
