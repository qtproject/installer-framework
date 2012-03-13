TEMPLATE = app
TARGET = repogen
DEPENDPATH += . .. ../common
INCLUDEPATH += . .. ../common

DESTDIR = ../bin

CONFIG += console
CONFIG -= app_bundle

QT += xml

include(../libinstaller/libinstaller.pri)

# Input
SOURCES += repogen.cpp \
           repositorygen.cpp
HEADERS += repositorygen.h
