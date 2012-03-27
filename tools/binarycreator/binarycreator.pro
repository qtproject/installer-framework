TEMPLATE = app
TARGET = binarycreator
DEPENDPATH += . .. rcc ../common
INCLUDEPATH += . .. rcc ../common

include(../../installerfw.pri)

QT -= gui
QT += script
LIBS += -linstaller

CONFIG += console
CONFIG -= app_bundle
DESTDIR = $$IFW_APP_PATH

SOURCES = binarycreator.cpp \
          rcc.cpp \
          rccmain.cpp \
          repositorygen.cpp
HEADERS = rcc.h
RESOURCES += binarycreator.qrc
