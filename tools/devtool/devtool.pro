TEMPLATE = app
TARGET = devtool

QT = core network qml xml concurrent

include(../../installerfw.pri)

CONFIG -= import_plugins

CONFIG += console
DESTDIR = $$IFW_APP_PATH

!macos:QTPLUGIN += qopensslbackend
macos:QTPLUGIN += qsecuretransportbackend

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
