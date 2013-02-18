TEMPLATE = app
INCLUDEPATH += . ..
TARGET = environmentvariable

include(../../installerfw.pri)

QT -= gui
QT += testlib

CONFIG += console
CONFIG -= app_bundle
DESTDIR = $$IFW_APP_PATH

HEADERS = environmentvariabletest.h 
SOURCES = environmentvariabletest.cpp
