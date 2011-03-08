QT       += core network xml

QT       -= gui

TARGET = repogenfromonlinerepo
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += main.cpp
HEADERS += downloadmanager.h
SOURCES += downloadmanager.cpp
HEADERS += textprogressbar.h
SOURCES += textprogressbar.cpp
