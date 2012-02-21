include(../../installerbuilder.pri)

TEMPLATE = app
TARGET = extractarchiveoperationtest

CONFIG -= app_bundle

QT += testlib script
QT -= gui

INCLUDEPATH += ../../libinstaller ..
DEPENDPATH += ../../libinstaller ../../common

SOURCES = extractarchiveoperationtest.cpp
HEADERS = extractarchiveoperationtest.h 
