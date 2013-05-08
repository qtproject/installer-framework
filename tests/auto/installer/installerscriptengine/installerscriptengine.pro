include(../../qttest.pri)

QT += script
greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
}

SOURCES += tst_installerscriptengine.cpp

RESOURCES += \
    installerscriptengine.qrc
