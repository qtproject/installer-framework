TEMPLATE = app
TARGET = archivegen
DEPENDPATH += . .. ../common
INCLUDEPATH += . ..

DESTDIR = ../bin

CONFIG += console
CONFIG -= app_bundle

QT += xml

include(../libinstaller/libinstaller.pri)

# Input
SOURCES += archive.cpp \
           ../common/repositorygen.cpp
HEADERS += ../common/repositorygen.h
