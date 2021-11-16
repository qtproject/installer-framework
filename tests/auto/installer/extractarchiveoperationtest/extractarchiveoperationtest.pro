include(../../qttest.pri)

QT -= gui
QT += testlib

RESOURCES += data.qrc \
    ../shared/config.qrc
SOURCES = tst_extractarchiveoperationtest.cpp
