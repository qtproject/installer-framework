include(../../qttest.pri)

QT -= gui
QT += testlib

SOURCES = tst_consumeoutputoperationtest.cpp

DEFINES += "QMAKE_BINARY=$$fromNativeSeparators($$QMAKE_BINARY)"
