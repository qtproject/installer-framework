TEMPLATE = app
INCLUDEPATH += . ..
TARGET = extractarchiveoperationtest

include(../../installerfw.pri)

QT -= gui
QT += testlib
LIBS += -linstaller

CONFIG += console
CONFIG -= app_bundle
DESTDIR = $$IFW_APP_PATH

HEADERS = extractarchiveoperationtest.h
SOURCES = extractarchiveoperationtest.cpp
