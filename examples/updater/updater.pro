TEMPLATE = app
TARGET = Updater

include( ../../installerbuilder/libinstaller/libinstaller.pri )
LIBS = -L$$OUT_PWD/../../installerbuilder/lib -linstaller $$LIBS

QT += gui

CONFIG += uitools

SOURCES += main.cpp updater.cpp
RESOURCES += updater.qrc
