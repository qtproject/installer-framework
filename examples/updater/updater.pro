TEMPLATE = app
TARGET = Updater

include( ../../installerbuilder/libinstaller/libinstaller.pri )
LIBS = -L$$OUT_PWD/../../installerbuilder/lib -linstaller $$LIBS

QT += gui

CONFIG += uitools

SOURCES += main.cpp
RESOURCES += updater.qrc
