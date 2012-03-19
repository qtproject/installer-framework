# Adapted from doc/doc.pri in Qt Creator.

QDOC_BIN = $$[QT_INSTALL_BINS]/qdoc3
win32:QDOC_BIN = $$replace(QDOC_BIN, "/", "\\")

unix {
    QDOC = SRCDIR=$$PWD OUTDIR=$$OUT_PWD/doc/html QT_MOBILITY_SOURCE_PATH=$$QT_MOBILITY_SOURCE_PATH $$QDOC_BIN
    HELPGENERATOR = $$[QT_INSTALL_BINS]/qhelpgenerator
} else {
    QDOC = set SRCDIR=$$PWD&& set OUTDIR=$$OUT_PWD/doc/html&& set QT_MOBILITY_SOURCE_PATH=$$QT_MOBILITY_SOURCE_PATH&& $$QDOC_BIN
    # Always run qhelpgenerator inside its own cmd; this is a workaround for
    # an unusual bug which causes qhelpgenerator.exe to do nothing
    HELPGENERATOR = cmd /C $$replace($$list($$[QT_INSTALL_BINS]/qhelpgenerator.exe), "/", "\\")
}

QHP_FILE = $$OUT_PWD/doc/html/installerfw.qhp
QCH_FILE = $$OUT_PWD/doc/installerfw.qch

HELP_DEP_FILES = $$PWD/installerfw.qdoc \
                 $$PWD/scripting.qdoc \
                 $$PWD/installerfw.qdocconf \
                 $$PWD/installerfw-online.qdocconf \
                 $$PWD/config/compat.qdocconf \
                 $$PWD/config/macros.qdocconf \
                 $$PWD/config/qt-cpp-ignore.qdocconf \
                 $$PWD/config/qt-defines.qdocconf \
                 $$PWD/config/qt-html-templates.qdocconf \
                 $$PWD/config/qt-html-default-styles.qdocconf

unix {
html_docs.commands = $$QDOC $$PWD/installerfw.qdocconf
} else {
html_docs.commands = \"$$QDOC $$PWD/installerfw.qdocconf\"
}
html_docs.depends += $$HELP_DEP_FILES
html_docs.files = $$QHP_FILE

unix {
html_docs_online.commands = $$QDOC $$PWD/installerfw-online.qdocconf
} else {
html_docs_online.commands = \"$$QDOC $$PWD/installerfw-online.qdocconf\"
}
html_docs_online.depends += $$HELP_DEP_FILES
html_docs_online.files = $$QHP_FILE

qch_docs.commands = $$HELPGENERATOR -o $$QCH_FILE $$QHP_FILE
qch_docs.depends += html_docs
qch_docs.files = $$QCH_FILE

unix:!macx {
    qch_docs.path = $$PREFIX/doc
    qch_docs.CONFIG += no_check_exist
    INSTALLS += qch_docs
}

docs_online.depends = html_docs_online
QMAKE_EXTRA_TARGETS += html_docs_online docs_online

macx {
    DOC_DIR = "$${OUT_PWD}/bin/Simulator.app/Contents/Resources/doc"
    cp_docs.commands = mkdir -p \"$${DOC_DIR}\" ; $${QMAKE_COPY} \"$${QCH_FILE}\" \"$${DOC_DIR}\"
    cp_docs.depends += qch_docs
    docs.depends = cp_docs
    QMAKE_EXTRA_TARGETS += html_docs qch_docs cp_docs docs ht
}
!macx {
    docs.depends = qch_docs
    QMAKE_EXTRA_TARGETS += html_docs qch_docs docs
}

OTHER_FILES = $$HELP_DEP_FILES

fixnavi.commands = \
    cd $$replace(PWD, "/", $$QMAKE_DIR_SEP) && \
    perl fixnavi.pl installerfw.qdoc .
QMAKE_EXTRA_TARGETS += fixnavi
