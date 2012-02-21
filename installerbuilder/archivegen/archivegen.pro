include(../installerbuilder.pri)

INCLUDEPATH += . ..
DEPENDPATH += . .. ../common

TEMPLATE = app
TARGET = archivegen

CONFIG += console
CONFIG -= app_bundle

QT += xml

# Input
SOURCES += archive.cpp \
           ../common/repositorygen.cpp
HEADERS += ../common/repositorygen.h
