include(../../qttest.pri)

QT += qml widgets

SOURCES += tst_scriptengine.cpp

DEFINES += "BUILDDIR=\\\"$$OUT_PWD\\\""

RESOURCES += \
    scriptengine.qrc
