TEMPLATE = app
TARGET = repogen
DEPENDPATH += . .. ../common
INCLUDEPATH += . ..

DESTDIR = ../bin

CONFIG += console
CONFIG -= app_bundle

QT += xml

include(../libinstaller/libinstaller.pri)

# Input
SOURCES += repogen.cpp \
           ../common/repositorygen.cpp
HEADERS += ../common/repositorygen.h 
