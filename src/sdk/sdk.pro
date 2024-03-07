TEMPLATE = app
INCLUDEPATH += . ..
TARGET = installerbase

include(../../installerfw.pri)

!isEmpty(SQUISH_PATH) {
    DEFINES += ENABLE_SQUISH
    include($$SQUISH_PATH/qtbuiltinhook.pri)
}

QT += network qml xml widgets concurrent
# add the minimal plugin in static build to be able to start the installer headless with:
# installer-binary --platform minimal
# using QT += qpa_minimal_plugin would result in a minimal only compiled version
!win32:CONFIG(static, static|shared) {
    QTPLUGIN += qminimal
}

CONFIG(static, static|shared) {
  # prevent qmake from automatically linking in bearer and qmltooling plugins
  QTPLUGIN.bearer = -
  QTPLUGIN.qmltooling = -
  # ICNS support required on macOS, prevent linking on other platforms
  !macos:QTPLUGIN.imageformats = -
}

DESTDIR = $$IFW_APP_PATH

exists($$LRELEASE) {
    IB_TRANSLATIONS = $$files($$PWD/translations/*_??.ts)
    IB_TRANSLATIONS -= $$PWD/translations/ifw_en.ts

    empty_ts = "<TS></TS>"
    write_file($$OUT_PWD/translations/ifw_en.ts, empty_ts)|error("Aborting.")
    IB_TRANSLATIONS += $$OUT_PWD/translations/ifw_en.ts
    QMAKE_DISTCLEAN += translations/ifw_en.ts

    qrc_cont = \
        "<RCC>" \
        "    <qresource prefix=\"/\">"
    for (file, IB_TRANSLATIONS) {
        lang = $$basename(file)
        lang = $$replace(lang, .ts, "")
        lang = $$replace(lang, ifw_, "")
        qlang = $${lang}
        qfile = $$[QT_INSTALL_TRANSLATIONS]/qtbase_$${lang}.qm
        !exists($$qfile) {
            qfile = $$[QT_INSTALL_TRANSLATIONS]/qt_$${qlang}.qm
            !exists($$qfile) {
                # get 'pt' from 'pt_BR', for example, to find 'qt_pt.qm' file
                qlang ~= s/_.*//
                qfile = $$[QT_INSTALL_TRANSLATIONS]/qtbase_$${lang}.qm
                !exists($$qfile) {
                    qfile = $$[QT_INSTALL_TRANSLATIONS]/qt_$${qlang}.qm
                    !exists($$qfile) {
                        warning("No Qt translation for '$$lang'; skipping.")
                        next()
                    }
                }
            }
        }

        qrc_cont += \
            "        <file>translations/ifw_$${lang}.qm</file>" \
            "        <file alias=\"translations/qt_$${qlang}.qm\">$$qfile</file>"
        ACTIVE_IB_TRANSLATIONS += $$file
        RESOURCE_DEPS += $$qfile translations/ifw_$${lang}.qm
    }
    qrc_cont += \
        "    </qresource>" \
        "</RCC>"
    RESOURCE = $$OUT_PWD/installerbase.qrc
    write_file($$RESOURCE, qrc_cont)|error("Aborting.")
    QMAKE_DISTCLEAN += $$RESOURCE

    !isEmpty(ACTIVE_IB_TRANSLATIONS) {
        updateqm.input = ACTIVE_IB_TRANSLATIONS
        updateqm.output = translations/${QMAKE_FILE_BASE}.qm
        updateqm.commands = $$LRELEASE ${QMAKE_FILE_IN} -qm ${QMAKE_FILE_OUT}
        updateqm.name = LRELEASE ${QMAKE_FILE_IN}
        updateqm.CONFIG += no_link target_predeps
        QMAKE_EXTRA_COMPILERS += updateqm

        exists($$RCC) {
            runrcc.input = RESOURCE
            runrcc.output = qrc_${QMAKE_FILE_BASE}.cpp
            runrcc.commands = $$RCC -name ${QMAKE_FILE_BASE} ${QMAKE_FILE_IN} -o ${QMAKE_FILE_OUT}
            runrcc.name = RCC ${QMAKE_FILE_IN}
            runrcc.CONFIG += no_link explicit_dependencies
            runrcc.depends = $$RESOURCE_DEPS
            runrcc.variable_out = SOURCES
            QMAKE_EXTRA_COMPILERS += runrcc
        }
    }
}

FORMS += settingsdialog.ui

HEADERS += \
           tabcontroller.h \
           installerbasecommons.h \
           settingsdialog.h \
           sdkapp.h \
           commandlineinterface.h \
           installerbase.h \
           aboutapplicationdialog.h

SOURCES += \
          main.cpp \
          installerbase.cpp \
          tabcontroller.cpp \
          installerbasecommons.cpp \
          settingsdialog.cpp \
          commandlineinterface.cpp \
          aboutapplicationdialog.cpp

win32 {
    # Use our own manifest file
    CONFIG -= embed_manifest_exe
    RC_FILE = installerbase.rc
}

macx:include(../../no_app_bundle.pri)

target.path = $$[QT_INSTALL_BINS]
INSTALLS += target
