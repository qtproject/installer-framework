include(../../qttest.pri)

QT -= gui
QT += testlib qml

SOURCES = tst_elevatedexecuteoperation.cpp

DEFINES += "QMAKE_BINARY=$$fromNativeSeparators($$QMAKE_BINARY)"
