TEMPLATE = aux

CONFIG += force_qt
QT += core-private widgets concurrent network qml xml

CONFIG += force_independent
QMAKE_DOCS_TARGETDIR = html

build_online_docs: \
    QMAKE_DOCS = $$PWD/ifw-online.qdocconf
else: \
    QMAKE_DOCS = $$PWD/ifw.qdocconf
