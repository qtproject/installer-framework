TEMPLATE = app
TARGET = archivegen
INCLUDEPATH += . ..

include(../../installerfw.pri)

QT -= gui
QT += qml xml concurrent

CONFIG -= import_plugins
CONFIG(lzmasdk) {
    LIBS += -l7z
}
CONFIG += console
DESTDIR = $$IFW_APP_PATH

SOURCES += archive.cpp

macx:include(../../no_app_bundle.pri)

target.path = $$[QT_INSTALL_BINS]
INSTALLS += target
