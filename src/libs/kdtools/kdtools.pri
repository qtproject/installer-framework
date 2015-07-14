INCLUDEPATH += $$PWD

DEFINES += BUILD_SHARED_KDTOOLS

FORMS += $$PWD/authenticationdialog.ui

HEADERS += $$PWD/kdtoolsglobal.h \
    $$PWD/job.h \
    $$PWD/genericfactory.h \
    $$PWD/selfrestarter.h \
    $$PWD/runoncechecker.h \
    $$PWD/lockfile.h \
    $$PWD/sysinfo.h

SOURCES += $$PWD/job.cpp \
    $$PWD/selfrestarter.cpp \
    $$PWD/runoncechecker.cpp \
    $$PWD/lockfile.cpp \
    $$PWD/sysinfo.cpp


HEADERS += $$PWD/updater.h \
    $$PWD/filedownloader.h \
    $$PWD/filedownloader_p.h \
    $$PWD/filedownloaderfactory.h \
    $$PWD/localpackagehub.h \
    $$PWD/update.h \
    $$PWD/updateoperation.h \
    $$PWD/updateoperationfactory.h \
    $$PWD/updateoperations.h \
    $$PWD/task.h \
    $$PWD/updatefinder.h \
    $$PWD/updatesinfo_p.h \
    $$PWD/environment.h \
    $$PWD/updatesinfodata_p.h

SOURCES += $$PWD/filedownloader.cpp \
    $$PWD/filedownloaderfactory.cpp \
    $$PWD/localpackagehub.cpp \
    $$PWD/update.cpp \
    $$PWD/updateoperation.cpp \
    $$PWD/updateoperationfactory.cpp \
    $$PWD/updateoperations.cpp \
    $$PWD/task.cpp \
    $$PWD/updatefinder.cpp \
    $$PWD/updatesinfo.cpp \
    $$PWD/environment.cpp

win32 {
    SOURCES += $$PWD/lockfile_win.cpp \
        $$PWD/kdsysinfo_win.cpp
}

unix {
    SOURCES += $$PWD/lockfile_unix.cpp
    osx: SOURCES += $$PWD/sysinfo_mac.cpp
    else: SOURCES += $$PWD/sysinfo_x11.cpp
}
