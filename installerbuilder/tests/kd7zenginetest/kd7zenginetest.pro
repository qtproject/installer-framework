TEMPLATE = app
TARGET = kd7zenginetest

DESTDIR = bin

CONFIG -= app_bundle

QT += testlib

include(../../libinstaller/libinstaller.pri)

SOURCES = kd7zenginetest.cpp 
HEADERS = kd7zenginetest.h 

win32:LIBS += ole32.lib oleaut32.lib user32.lib
win32:OBJECTS_DIR = .obj
