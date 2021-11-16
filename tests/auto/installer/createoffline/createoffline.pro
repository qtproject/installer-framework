include(../../qttest.pri)

QT -= gui
QT += testlib

SOURCES = tst_createoffline.cpp

RESOURCES += settings.qrc \
    ../shared/config.qrc
