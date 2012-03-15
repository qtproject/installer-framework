TEMPLATE = app
DEPENDPATH += . ..
INCLUDEPATH += . ..
TARGET = repogenfromonlinerepo

include(../../installerfw.pri)

QT -= gui
QT += xml network

CONFIG += console
CONFIG -= app_bundle
DESTDIR = $$IFW_APP_PATH

SOURCES += main.cpp \
        downloadmanager.cpp \
        textprogressbar.cpp

HEADERS += downloadmanager.h \
        textprogressbar.h
