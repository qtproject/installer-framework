include(../../qttest.pri)

QT -= gui
QT += testlib network

SOURCES = tst_registerfiletypeoperation.cpp

RESOURCES += \
    settings.qrc \
    ..\shared\config.qrc
