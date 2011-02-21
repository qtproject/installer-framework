TEMPLATE = app
TARGET = installerbase

DEPENDPATH += . ..
INCLUDEPATH += . .. 

DEFINES += QT_NO_CAST_FROM_ASCII

win32:RC_FILE = installerbase.rc

DESTDIR = ../bin

CONFIG += help
QTPLUGIN += qsqlite

CONFIG -= app_bundle

include(../libinstaller/libinstaller.pri)

QT += network

HEADERS += tabcontroller.h \
           installerbasecommons.h \
           maintabwidget.h

SOURCES = installerbase.cpp \
          tabcontroller.cpp \
          installerbasecommons.cpp \
          maintabwidget.cpp

RESOURCES += installerbase.qrc

win32-msvc2005 {
  CONFIG += embed_manifest_exe #msvc2008 is doing this automaticaly
}

embed_manifest_exe:win32-msvc2005 {
    # The default configuration embed_manifest_exe overrides the manifest file
    # already embedded via RC_FILE. Vs2008 already have the necessary manifest entry
  QMAKE_POST_LINK += $$quote(mt.exe -updateresource:$$DESTDIR/$${TARGET}.exe -manifest \"$${PWD}\\$${TARGET}.exe.manifest\")
}
