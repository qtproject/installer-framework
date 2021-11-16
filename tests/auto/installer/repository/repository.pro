include(../../qttest.pri)

QT += network
QT -= gui

SOURCES += tst_repository.cpp

RESOURCES += \
    settings.qrc \
    ../shared/config.qrc
