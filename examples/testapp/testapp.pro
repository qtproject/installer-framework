TEMPLATE = app
INCLUDEPATH += . ..
TARGET = testapp

include(../../installerfw.pri)

isEqual(QT_MAJOR_VERSION, 5) {
    QT += widgets
}

!static {
    warning("You can use this example only with a static build of Qt and IFW!")
}

DESTDIR = $$IFW_BUILD_TREE/examples/testapp/packages/com.nokia.testapp/data

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

macx {
    QMAKE_POST_LINK = ($$IFW_APP_PATH/binarycreator -p $$IFW_SOURCE_TREE/examples/testapp/packages \
        -c $$IFW_SOURCE_TREE/examples/testapp/config/config.xml -t $$IFW_APP_PATH/installerbase \
         TestAppInstaller.app
} win32: {
    QMAKE_POST_LINK = ($$IFW_APP_PATH/binarycreator.exe -p $$IFW_SOURCE_TREE/examples/testapp/packages \
        -c $$IFW_SOURCE_TREE/examples/testapp/config/config.xml -t $$IFW_APP_PATH/installerbase.exe \
         TestAppInstaller.exe)
} else {
    QMAKE_POST_LINK = ($$IFW_APP_PATH/binarycreator -p $$IFW_SOURCE_TREE/examples/testapp/packages \
        -c $$IFW_SOURCE_TREE/examples/testapp/config/config.xml -t $$IFW_APP_PATH/installerbase \
         TestAppInstaller)
}
