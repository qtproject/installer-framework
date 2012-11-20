TEMPLATE = app
TARGET = archivegen
INCLUDEPATH += . .. ../common

include(../../installerfw.pri)

QT -= gui
QT += script

CONFIG += console
DESTDIR = $$IFW_APP_PATH

SOURCES += archive.cpp \
        ../common/repositorygen.cpp
HEADERS += ../common/repositorygen.h
