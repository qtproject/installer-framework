TEMPLATE = app
INCLUDEPATH += . ..
TARGET = repocompare

include(../../installerfw.pri)

QT += widgets network

SOURCES += main.cpp\
        mainwindow.cpp \
        repositorymanager.cpp

HEADERS += mainwindow.h \
        repositorymanager.h

FORMS += mainwindow.ui

osx:include(../../no_app_bundle.pri)
