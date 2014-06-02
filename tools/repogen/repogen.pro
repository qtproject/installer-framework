TEMPLATE = app
TARGET = repogen
INCLUDEPATH += . .. ../common

include(../../installerfw.pri)

QT -= gui
QT += qml xml

CONFIG += console
DESTDIR = $$IFW_APP_PATH

SOURCES += repogen.cpp \
           ../common/repositorygen.cpp
HEADERS += ../common/repositorygen.h

macx:include(../../no_app_bundle.pri)
