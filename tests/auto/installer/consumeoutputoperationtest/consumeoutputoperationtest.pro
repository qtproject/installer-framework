include(../../qttest.pri)

QT -= gui
QT += testlib qml

SOURCES = tst_consumeoutputoperationtest.cpp

DEFINES += "QMAKE_BINARY=$$fromNativeSeparators($$QMAKE_BINARY)"

RESOURCES += \
    settings.qrc \
    ../shared/config.qrc
