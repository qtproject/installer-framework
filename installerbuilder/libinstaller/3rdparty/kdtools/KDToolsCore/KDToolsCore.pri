DEPENDPATH += $$PWD
INCLUDEPATH += $$PWD

CONFIG( shared, static|shared) {
    DEFINES += BUILD_SHARED_KDTOOLSCORE
}

HEADERS += $$PWD/pimpl_ptr.h \
    $$PWD/kdtoolsglobal.h \
    $$PWD/kdbytesize.h \
    $$PWD/kdjob.h \
    $$PWD/kdgenericfactory.h \
    $$PWD/kdautopointer.h \
    $$PWD/kdselfrestarter.h \
    $$PWD/kdsavefile.h \
    $$PWD/kdrunoncechecker.h \
    $$PWD/kdlockfile.h \
    $$PWD/kdsysinfo.h

SOURCES += $$PWD/pimpl_ptr.cpp \
    $$PWD/kdtoolsglobal.cpp \
    $$PWD/kdbytesize.cpp \
    $$PWD/kdjob.cpp \
    $$PWD/kdgenericfactory.cpp \
    $$PWD/kdautopointer.cpp \
    $$PWD/kdselfrestarter.cpp \
    $$PWD/kdsavefile.cpp \
    $$PWD/kdrunoncechecker.cpp \
    $$PWD/kdlockfile.cpp \
    $$PWD/kdsysinfo.cpp

unix:SOURCES += $$PWD/kdlockfile_unix.cpp
win32:SOURCES += $$PWD/kdlockfile_win.cpp
win32:SOURCES += $$PWD/kdsysinfo_win.cpp
macx:SOURCES += $$PWD/kdsysinfo_mac.cpp
unix:!macx:SOURCES += $$PWD/kdsysinfo_x11.cpp
