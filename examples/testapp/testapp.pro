include( ../../installerbuilder/installerbuilder.pri )

TEMPLATE = app
DESTDIR = packages/com.nokia.testapp/data

CONFIG += uitools help
QT += script network xml sql

# Input
HEADERS += mainwindow.h \
    componentselectiondialog.h \
    updatesettingsdialog.h \
    updateagent.h \
    updatesettingswidget.h

SOURCES += main.cpp mainwindow.cpp \
    componentselectiondialog.cpp \
    updatesettingsdialog.cpp \
    updateagent.cpp \
    updatesettingswidget.cpp
RESOURCES += testapp.qrc
FORMS += componentselectiondialog.ui updatesettingsdialog.ui updatesettingswidget.ui

macx:QMAKE_POST_LINK = ($$OUT_PWD/../../installerbuilder/bin/binarycreator -p packages -c config -t ../../installerbuilder/bin/installerbase TestAppInstaller.app com.nokia.testapp)
win32:QMAKE_POST_LINK = ($$OUT_PWD\\..\\..\\installerbuilder\\bin\\binarycreator.exe -p $$PWD\\packages -c $$PWD\\config -t $$OUT_PWD\\..\\..\\installerbuilder\\bin\\installerbase.exe TestAppInstaller.exe com.nokia.testapp)
