TEMPLATE = app
INCLUDEPATH += . ..
TARGET = downloadspeed

include(../../installerfw.pri)

QT -= gui
QT += network
LIBS += -linstaller

CONFIG += console
CONFIG -= app_bundle
DESTDIR = $$IFW_APP_PATH

SOURCES += main.cpp
