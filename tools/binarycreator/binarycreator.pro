TEMPLATE = app
TARGET = binarycreator
INCLUDEPATH += . ..

include(../../installerfw.pri)

QT -= gui
QT += qml xml

CONFIG -= import_plugins

CONFIG += console
DESTDIR = $$IFW_APP_PATH
SOURCES = main.cpp

macx:include(../../no_app_bundle.pri)

target.path = $$[QT_INSTALL_BINS]
INSTALLS += target
