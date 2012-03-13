TEMPLATE = app
TARGET = archivegen
DEPENDPATH += . .. ../common
INCLUDEPATH += . .. ../common

DESTDIR = ../bin

CONFIG += console
CONFIG -= app_bundle

QT += xml

include(../libinstaller/libinstaller.pri)

# Input
SOURCES += archive.cpp \
           repositorygen.cpp
HEADERS += repositorygen.h
