include(../installerbuilder.pri)

TEMPLATE = app
TARGET = repogen

INCLUDEPATH += . ..
DEPENDPATH += . .. ../common

CONFIG += console
CONFIG -= app_bundle

QT += xml

# Input
SOURCES += repogen.cpp \
           ../common/repositorygen.cpp
HEADERS += ../common/repositorygen.h 
