include(../../../installerfw.pri)

QT =
TARGET = 7z
TEMPLATE = lib
INCLUDEPATH += . ..
CONFIG += staticlib
DESTDIR = $$IFW_LIB_PATH

include(7zip.pri)
win32 {
    DEFINES += _CRT_SECURE_NO_WARNINGS
    CONFIG += no_batch # this is needed because we have a same named *.c and *.cpp file -> 7in
    include($$7ZIP_BASE/win.pri)    #this is 7zip
}

unix {
    QMAKE_CFLAGS += -w
    QMAKE_CXXFLAGS += -fvisibility=hidden -w
    include($$7ZIP_BASE/unix.pri)   #this is p7zip
}
