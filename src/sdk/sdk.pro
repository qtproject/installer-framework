TEMPLATE = app
DEPENDPATH += . ..
INCLUDEPATH += . .. 
TARGET = installerbase

include(../../installerfw.pri)

LIBS += -linstaller
QT += network script

CONFIG -= app_bundle
DESTDIR = $$IFW_APP_PATH

if (exists($$LRELEASE)) {
    QT_LANGUAGES = qt_de
    IB_LANGUAGES = de_de en_us
    defineReplace(prependAll) {
        for(a,$$1):result += $$2$${a}$$3
        return($$result)
    }

    defineTest(testFiles) {
        for(file, $$1) {
            !exists($$file):return(false)
        }
        return(true)
    }

    SUCCESS = false
    IB_TRANSLATIONS = $$prependAll(IB_LANGUAGES, $$PWD/translations/,.ts)
    QT_TRANSLATIONS = $$prependAll(QT_LANGUAGES, $$[QT_INSTALL_TRANSLATIONS]/,.ts)

    if (!testFiles(QT_TRANSLATIONS)) {
        QT_COMPILED_TRANSLATIONS = $$prependAll(QT_LANGUAGES, $$[QT_INSTALL_TRANSLATIONS]/,.qm)
        if (testFiles(QT_COMPILED_TRANSLATIONS)) {
            SUCCESS = true
            copyqm.input = QT_COMPILED_TRANSLATIONS
            copyqm.output = $$PWD/translations/${QMAKE_FILE_BASE}.qm
            unix:copyqm.commands = $$QMAKE_COPY ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
            win32:copyqm.commands = $$QMAKE_COPY \"${QMAKE_FILE_IN}\" \"${QMAKE_FILE_OUT}\"
            copyqm.name = COPY ${QMAKE_FILE_IN}
            copyqm.CONFIG += no_link target_predeps
            QMAKE_EXTRA_COMPILERS += copyqm
        }
    } else {
        SUCCESS = true
        IB_TRANSLATIONS += $$QT_TRANSLATIONS
    }

    if (contains(SUCCESS, true)) {
        updateqm.input = IB_TRANSLATIONS
        updateqm.output = $$PWD/translations/${QMAKE_FILE_BASE}.qm
        updateqm.commands = $$LRELEASE ${QMAKE_FILE_IN} -qm ${QMAKE_FILE_OUT}
        updateqm.name = LRELEASE ${QMAKE_FILE_IN}
        updateqm.CONFIG += no_link target_predeps
        QMAKE_EXTRA_COMPILERS += updateqm

        if (exists($$RCC)) {
            RESOURCE_IB_TRANSLATIONS = $$prependAll(IB_LANGUAGES, $$PWD/translations/,.qm)
            RESOURCE_QT_TRANSLATIONS = $$prependAll(QT_LANGUAGES, $$PWD/translations/,.qm)
            RESOURCE = $$PWD/installerbase.qrc
            runrcc.input = RESOURCE
            runrcc.output = $$PWD/qrc_installerbase.cpp
            runrcc.commands = $$RCC -name ${QMAKE_FILE_BASE} ${QMAKE_FILE_IN} -o ${QMAKE_FILE_OUT}
            runrcc.name = RCC ${QMAKE_FILE_IN}
            runrcc.CONFIG += no_link explicit_dependencies
            runrcc.depends = $$RESOURCE_IB_TRANSLATIONS $$RESOURCE_QT_TRANSLATIONS
            runrcc.variable_out = SOURCES
            QMAKE_EXTRA_COMPILERS += runrcc
        }
    }
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
  CONFIG += embed_manifest_exe #msvc2008 is doing this automatically
}

embed_manifest_exe:win32-msvc2005 {
    # The default configuration embed_manifest_exe overrides the manifest file
    # already embedded via RC_FILE. Vs2008 already have the necessary manifest entry
    QMAKE_POST_LINK += $$quote(mt.exe -updateresource:$$IFW_APP_PATH/$${TARGET}.exe -manifest \"$${IFW_SOURCE_TREE}\\src\\sdk\\$${TARGET}.exe.manifest\")
}
