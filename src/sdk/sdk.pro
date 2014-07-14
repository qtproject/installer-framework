TEMPLATE = app
INCLUDEPATH += . .. 
TARGET = installerbase

include(../../installerfw.pri)

QT += network qml xml widgets
# add the minimal plugin in static build to be able to start the installer headless with:
# installer-binary -platform minimal
# using QT += qpa_minimal_plugin would result in a minimal only compiled version
!win32:CONFIG(static, static|shared) {
    QTPLUGIN += qminimal
}

DESTDIR = $$IFW_APP_PATH

exists($$LRELEASE) {
    QT_LANGUAGES = qt_de qt_ru qt_zh_CN qt_ja
    IB_LANGUAGES = de_de en_us ru_ru zh_cn ja_jp
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

    wd = $$toNativeSeparators($$IFW_SOURCE_TREE)
    sources = src
    lupdate_opts = -locations relative -no-ui-lines -no-sort

    for(file, IB_TRANSLATIONS) {
        lang = $$replace(file, .*/([^/]*)\\.ts, \\1)
        v = ts-$${lang}.commands
        $$v = cd $$wd && $$LUPDATE $$lupdate_opts $$sources -ts $$file
        QMAKE_EXTRA_TARGETS += ts-$$lang
    }
    ts-all.commands = cd $$wd && $$LUPDATE $$lupdate_opts $$sources -ts $$IB_TRANSLATIONS
    QMAKE_EXTRA_TARGETS += ts-all

    isEqual(QMAKE_DIR_SEP, /) {
        commit-ts.commands = \
            cd $$wd; \
            git add -N src/sdk/translations/*_??.ts && \
            for f in `git diff-files --name-only src/sdk/translations/*_??.ts`; do \
                $$LCONVERT -locations none -i \$\$f -o \$\$f; \
            done; \
            git add src/sdk/translations/*_??.ts && git commit
    } else {
        commit-ts.commands = \
            cd $$wd && \
            git add -N src/sdk/translations/*_??.ts && \
            for /f usebackq %%f in (`git diff-files --name-only src/sdk/translations/*_??.ts`) do \
                $$LCONVERT -locations none -i %%f -o %%f $$escape_expand(\\n\\t) \
            cd $$wd && git add src/sdk/translations/*_??.ts && git commit
    }
    QMAKE_EXTRA_TARGETS += commit-ts

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

        exists($$RCC) {
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

HEADERS += \
           tabcontroller.h \
           installerbasecommons.h \
           settingsdialog.h \
           console.h \
           sdkapp.h \
           updatechecker.h

SOURCES = installerbase.cpp \
          tabcontroller.cpp \
          installerbasecommons.cpp \
          settingsdialog.cpp \
          updatechecker.cpp

win32 {
    # Force to overwrite the default manifest file with our own extended version.
    RC_FILE = installerbase_qt5.rc
    QMAKE_MANIFEST = installerbase.manifest
}

macx:include(../../no_app_bundle.pri)
