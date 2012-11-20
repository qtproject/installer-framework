TEMPLATE = app
INCLUDEPATH += . ..
TARGET = testapp

include(../../installerfw.pri)

!static {
    warning("You can use this example only with a static build of Qt and IFW!")
}

LIBS += -linstaller
DESTDIR = packages/com.nokia.testapp/data

FORMS += \
        componentselectiondialog.ui \
        updatesettingsdialog.ui \
        updatesettingswidget.ui

HEADERS += mainwindow.h \
        componentselectiondialog.h \
        updatesettingsdialog.h \
        updateagent.h \
        updatesettingswidget.h

SOURCES += main.cpp \
        mainwindow.cpp \
        componentselectiondialog.cpp \
        updatesettingsdialog.cpp \
        updateagent.cpp \
        updatesettingswidget.cpp

RESOURCES += testapp.qrc

isEqual(IFW_SOURCE_TREE, $$IFW_BUILD_TREE) {
    macx:QMAKE_POST_LINK = ($$IFW_APP_PATH/binarycreator -p $$PWD/packages -c $$PWD/config -t $$IFW_APP_PATH/installerbase TestAppInstaller.app)
    win32:QMAKE_POST_LINK = ($$IFW_APP_PATH/binarycreator.exe -p $$PWD/packages -c $$PWD/config -t $$IFW_APP_PATH/installerbase.exe TestAppInstaller.exe)
}
