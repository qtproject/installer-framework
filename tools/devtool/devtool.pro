TEMPLATE = app
TARGET = devtool

include(../../installerfw.pri)

QT -= gui
QT += network xml

CONFIG += console
DESTDIR = $$IFW_APP_PATH

HEADERS += operationrunner.h \
    binaryreplace.h \
    binarydump.h

SOURCES += main.cpp \
    operationrunner.cpp \
    binaryreplace.cpp \
    binarydump.cpp

osx:include(../../no_app_bundle.pri)

target.path = $$[QT_INSTALL_BINS]
INSTALLS += target
