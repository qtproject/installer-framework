# Adapted from doc/doc.pri in Qt Creator.

QDOC_BIN = $$[QT_INSTALL_BINS]/qdoc
win32:QDOC_BIN = $$replace(QDOC_BIN, "/", "\\")

IFW_VERSION_TAG = $$replace(IFW_VERSION_STR, "[-.]", )

defineReplace(cmdEnv) {
    !equals(QMAKE_DIR_SEP, /): 1 ~= s,^(.*)$,(set \\1) &&,g
    return("$$1")
}

QDOC = $$cmdEnv(SRCDIR=$$PWD OUTDIR=$$OUT_PWD/doc/html IFW_VERSION=$$IFW_VERSION_STR IFW_VERSION_TAG=$$IFW_VERSION_TAG QT_INSTALL_DOCS=$$[QT_INSTALL_DOCS]) $$QDOC_BIN

unix {
    HELPGENERATOR = $$[QT_INSTALL_BINS]/qhelpgenerator
} else {
    # Always run qhelpgenerator inside its own cmd; this is a workaround for
    # an unusual bug which causes qhelpgenerator.exe to do nothing
    HELPGENERATOR = cmd /C $$replace($$list($$[QT_INSTALL_BINS]/qhelpgenerator.exe), "/", "\\")
}
HELPGENERATOR = $$HELPGENERATOR -platform minimal

QHP_FILE = $$OUT_PWD/doc/html/ifw.qhp
QCH_FILE = $$OUT_PWD/doc/ifw.qch

html_docs.commands = $$QDOC $$PWD/installerfw.qdocconf
html_docs.files = $$QHP_FILE

html_docs_online.commands = $$QDOC $$PWD/installerfw-online.qdocconf
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
    DOC_DIR = "$${OUT_PWD}/bin/Ifw.app/Contents/Resources/doc"
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
