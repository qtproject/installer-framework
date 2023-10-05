TEMPLATE = app
TARGET = repogen

INCLUDEPATH += . ..
include(../../installerfw.pri)

QT -= gui
QT += qml xml concurrent

CONFIG -= import_plugins

CONFIG += console
DESTDIR = $$IFW_APP_PATH

!macos:QTPLUGIN += qopensslbackend
macos:QTPLUGIN += qsecuretransportbackend

SOURCES += repogen.cpp

macx:include(../../no_app_bundle.pri)

target.path = $$[QT_INSTALL_BINS]
INSTALLS += target
