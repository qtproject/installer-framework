TEMPLATE = app
INCLUDEPATH += . ..
TARGET = extractbinarydata

include(../../installerfw.pri)

QT -= gui
LIBS += -linstaller

CONFIG += console
CONFIG -= app_bundle
DESTDIR = $$IFW_APP_PATH

SOURCES += main.cpp
