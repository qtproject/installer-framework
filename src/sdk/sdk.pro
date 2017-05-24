TEMPLATE = app
INCLUDEPATH += . .. 
TARGET = installerbase

include(../../installerfw.pri)

QT += network qml xml widgets
# add the minimal plugin in static build to be able to start the installer headless with:
# installer-binary --platform minimal
# using QT += qpa_minimal_plugin would result in a minimal only compiled version
!win32:CONFIG(static, static|shared) {
    QTPLUGIN += qminimal
}

CONFIG(static, static|shared) {
  # prevent qmake from automatically linking in imageformats, bearer, qmltooling plugins
  QTPLUGIN.imageformats = -
  QTPLUGIN.bearer = -
  QTPLUGIN.qmltooling = -
}

DESTDIR = $$IFW_APP_PATH

exists($$LRELEASE) {
    IB_TRANSLATIONS = $$files($$PWD/translations/??.ts) $$files($$PWD/translations/??_??.ts)
    IB_TRANSLATIONS -= $$PWD/translations/en.ts

    wd = $$toNativeSeparators($$IFW_SOURCE_TREE)
    sources = src
    lupdate_opts = -locations relative -no-ui-lines -no-sort

    IB_ALL_TRANSLATIONS = $$IB_TRANSLATIONS $$PWD/translations/untranslated.ts
    for(file, IB_ALL_TRANSLATIONS) {
        lang = $$replace(file, .*/([^/]*)\\.ts, \\1)
        v = ts-$${lang}.commands
        $$v = cd $$wd && $$LUPDATE $$lupdate_opts $$sources -ts $$file
        QMAKE_EXTRA_TARGETS += ts-$$lang
    }
    ts-all.commands = cd $$wd && $$LUPDATE $$lupdate_opts $$sources -ts $$IB_ALL_TRANSLATIONS
    QMAKE_EXTRA_TARGETS += ts-all

    isEqual(QMAKE_DIR_SEP, /) {
        commit-ts.commands = \
            cd $$wd; \
            git add -N src/sdk/translations/??.ts src/sdk/translations/??_??.ts && \
            for f in `git diff-files --name-only src/sdk/translations/??.ts src/sdk/translations/??_??.ts`; do \
                $$LCONVERT -locations none -i \$\$f -o \$\$f; \
            done; \
            git add src/sdk/translations/??.ts src/sdk/translations/??_??.ts && git commit
    } else {
        commit-ts.commands = \
            cd $$wd && \
            git add -N src/sdk/translations/??.ts src/sdk/translations/??_??.ts && \
            for /f usebackq %%f in (`git diff-files --name-only src/sdk/translations/??.ts src/sdk/translations/??_??.ts`) do \
                $$LCONVERT -locations none -i %%f -o %%f $$escape_expand(\\n\\t) \
            cd $$wd && git add src/sdk/translations/??.ts src/sdk/translations/??_??.ts && git commit
    }
    QMAKE_EXTRA_TARGETS += commit-ts

    empty_ts = "<TS></TS>"
    write_file($$OUT_PWD/translations/en.ts, empty_ts)|error("Aborting.")
    IB_TRANSLATIONS += $$OUT_PWD/translations/en.ts
    QMAKE_DISTCLEAN += translations/en.ts

    qrc_cont = \
        "<RCC>" \
        "    <qresource prefix=\"/\">"
    for (file, IB_TRANSLATIONS) {
        lang = $$replace(file, .*/([^/]*)\\.ts, \\1)
        qfile = $$[QT_INSTALL_TRANSLATIONS]/qtbase_$${lang}.qm
        !exists($$qfile) {
            qfile = $$[QT_INSTALL_TRANSLATIONS]/qt_$${lang}.qm
            !exists($$qfile) {
                warning("No Qt translation for '$$lang'; skipping.")
                next()
            }
        }
        qrc_cont += \
            "        <file>translations/$${lang}.qm</file>" \
            "        <file alias=\"translations/qt_$${lang}.qm\">$$qfile</file>"
        ACTIVE_IB_TRANSLATIONS += $$file
        RESOURCE_DEPS += $$qfile translations/$${lang}.qm
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
           console.h \
           sdkapp.h \
           updatechecker.h \
           installerbase.h \
           constants.h \
           commandlineparser.h

SOURCES = \
          main.cpp \
          installerbase.cpp \
          tabcontroller.cpp \
          installerbasecommons.cpp \
          settingsdialog.cpp \
          updatechecker.cpp \
          commandlineparser.cpp

win32 {
    # Use our own manifest file
    CONFIG -= embed_manifest_exe
    RC_FILE = installerbase.rc

    SOURCES += console_win.cpp
}

macx:include(../../no_app_bundle.pri)

target.path = $$[QT_INSTALL_BINS]
INSTALLS += target
