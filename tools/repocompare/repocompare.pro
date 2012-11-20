TEMPLATE = app
INCLUDEPATH += . ..
TARGET = repocompare

include(../../installerfw.pri)

QT += network
CONFIG -= app_bundle
DESTDIR = $$IFW_APP_PATH

SOURCES += main.cpp\
        mainwindow.cpp \
        repositorymanager.cpp

HEADERS += mainwindow.h \
        repositorymanager.h

FORMS += mainwindow.ui
