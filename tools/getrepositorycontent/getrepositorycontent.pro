TEMPLATE = app
INCLUDEPATH += . ..
TARGET = getrepositorycontent

include(../../installerfw.pri)

QT -= gui
QT += network

CONFIG += console
DESTDIR = $$IFW_APP_PATH

SOURCES += main.cpp \
        textprogressbar.cpp \
    downloader.cpp \
    domnodedebugstreamoperator.cpp

HEADERS += \
        textprogressbar.h \
    downloader.h \
    domnodedebugstreamoperator.h

macx:include(../../no_app_bundle.pri)
