QT += widgets concurrent network qml xml

DOC_TARGETDIR = html
INSTALL_DOC_PATH = $$IFW_BUILD_TREE/doc/$$DOC_TARGETDIR

build_online_docs: \
    DOC_FILES = $$PWD/ifw-online.qdocconf
else: \
    DOC_FILES = $$PWD/ifw.qdocconf

qtdocs.name = QT_INSTALL_DOCS
qtdocs.value = $$[QT_INSTALL_DOCS/src]
qdocindex.name = QDOC_INDEX_DIR
qdocindex.value = $$[QT_INSTALL_DOCS]
qtver.name = QT_VERSION
qtver.value = $$VERSION
qtvertag.name = QT_VERSION_TAG
qtvertag.value = $$replace(VERSION, \.,)
QDOC_ENV += \
            qtdocs \
            qdocindex \
            qtver \
            qtvertag

DOC_HTML_INSTALLDIR = $$INSTALL_DOC_PATH
DOC_QCH_OUTDIR = $$IFW_BUILD_TREE/doc
DOC_QCH_INSTALLDIR = $$INSTALL_DOC_PATH

for (include_path, INCLUDEPATH): \
    DOC_INCLUDES += -I $$shell_quote($$include_path)
for (module, QT) {
    MOD = $$replace(module, \-,_)
    MOD_INCLUDES = $$eval(QT.$${MOD}.includes)
    for (include_path, MOD_INCLUDES): \
        DOC_INCLUDES += -I $$shell_quote($$include_path)
}
for (include_path, QMAKE_DEFAULT_INCDIRS): \
    DOC_INCLUDES += -I $$shell_quote($$include_path)
macos: DOC_INCLUDES += -F $$shell_quote($$[QT_INSTALL_LIBS])

include(doc_targets.pri)
