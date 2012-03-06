TEMPLATE = app
TARGET = installerbase

DEPENDPATH += . ..
INCLUDEPATH += . .. 

GIT_SHA1 = $$system(git rev-list --abbrev-commit -n1 HEAD)

DEFINES += QT_NO_CAST_FROM_ASCII "_GIT_SHA1_=$$GIT_SHA1"

win32:RC_FILE = installerbase.rc

DESTDIR = ../bin

CONFIG += help

CONFIG -= app_bundle

include(../libinstaller/libinstaller.pri)

QM_FILES = qt_de.qm de_de.qm en_us.qm
defineTest(testQmFiles) {
    for(file, QM_FILES) {
        !exists($$PWD/translations/$$file) {
            message("File $$PWD/translations/$$file not found!")
            return(false)
        }
    }
    return(true)
}

if (testQmFiles()) {
RESOURCES += installerbase.qrc
}


QT += network

FORMS += settingsdialog.ui

HEADERS += installerbase_p.h \
           tabcontroller.h \
           installerbasecommons.h \
           settingsdialog.h

SOURCES = installerbase.cpp \
          installerbase_p.cpp \
          tabcontroller.cpp \
          installerbasecommons.cpp \
          settingsdialog.cpp

win32-msvc2005 {
  CONFIG += embed_manifest_exe #msvc2008 is doing this automaticaly
}

embed_manifest_exe:win32-msvc2005 {
    # The default configuration embed_manifest_exe overrides the manifest file
    # already embedded via RC_FILE. Vs2008 already have the necessary manifest entry
  QMAKE_POST_LINK += $$quote(mt.exe -updateresource:$$DESTDIR/$${TARGET}.exe -manifest \"$${PWD}\\$${TARGET}.exe.manifest\")
}
