include(../../qttest.pri)

QT += script
lessThan(QT_MAJOR_VERSION, 5) {
    QT -= gui
}
SOURCES += tst_packagemanagercore.cpp

RESOURCES += \
    settings.qrc
