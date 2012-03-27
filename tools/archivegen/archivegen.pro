TEMPLATE = app
TARGET = archivegen
DEPENDPATH += . .. ../common
INCLUDEPATH += . .. ../common

include(../../installerfw.pri)

QT -= gui
QT += script
LIBS += -linstaller

CONFIG += console
CONFIG -= app_bundle
DESTDIR = $$IFW_APP_PATH

SOURCES += archive.cpp \
        repositorygen.cpp
HEADERS += repositorygen.h
