TEMPLATE = app
INCLUDEPATH += . ..
TARGET = extractbinarydata

include(../../installerfw.pri)

QT -= gui
LIBS += -linstaller -l7z

CONFIG += console
DESTDIR = $$IFW_APP_PATH

SOURCES += main.cpp

macx:include(../../no_app_bundle.pri)
