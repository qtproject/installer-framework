TEMPLATE = app
TARGET = repogen
DEPENDPATH += . .. ../common
INCLUDEPATH += . .. ../common

include(../../installerfw.pri)

QT -= gui
QT += script

CONFIG += console
CONFIG -= app_bundle
DESTDIR = $$IFW_APP_PATH

SOURCES += repogen.cpp \
           repositorygen.cpp
HEADERS += repositorygen.h
