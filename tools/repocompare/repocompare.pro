TEMPLATE = app
DEPENDPATH += . ..
INCLUDEPATH += . ..
TARGET = repocompare

include(../../installerfw.pri)

QT += network
DESTDIR = $$IFW_APP_PATH

SOURCES += main.cpp\
        mainwindow.cpp \
        repositorymanager.cpp

HEADERS += mainwindow.h \
        repositorymanager.h

FORMS += mainwindow.ui
