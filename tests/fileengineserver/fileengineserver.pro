TEMPLATE = app
INCLUDEPATH += . ..
TARGET = fileengineserver

include(../../installerfw.pri)

QT -= gui
LIBS += -linstaller

CONFIG += console
CONFIG -= app_bundle
DESTDIR = $$IFW_APP_PATH

SOURCES += fileengineserver.cpp
