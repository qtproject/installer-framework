TEMPLATE = app
DEPENDPATH += . ..
INCLUDEPATH += . .. 
TARGET = installerbase

include(../../installerfw.pri)

LIBS += -linstaller
QT += network script

CONFIG -= app_bundle
DESTDIR = $$IFW_APP_PATH

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

win32:RC_FILE = installerbase.rc
win32-msvc2005 {
  CONFIG += embed_manifest_exe #msvc2008 is doing this automaticaly
}

embed_manifest_exe:win32-msvc2005 {
    # The default configuration embed_manifest_exe overrides the manifest file
    # already embedded via RC_FILE. Vs2008 already have the necessary manifest entry
    QMAKE_POST_LINK += $$quote(mt.exe -updateresource:$$IFW_APP_PATH/$${TARGET}.exe -manifest \"$${IFW_SOURCE_TREE}\\src\\sdk\\$${TARGET}.exe.manifest\")
}
