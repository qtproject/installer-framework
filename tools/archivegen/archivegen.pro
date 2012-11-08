TEMPLATE = app
TARGET = archivegen
DEPENDPATH += . .. ../common
INCLUDEPATH += . .. ../common

include(../../installerfw.pri)

QT -= gui
QT += script

CONFIG += console
DESTDIR = $$IFW_APP_PATH

SOURCES += archive.cpp \
        repositorygen.cpp
HEADERS += repositorygen.h
