INCLUDEPATH += $$PWD

DEFINES += BUILD_SHARED_KDTOOLS

FORMS += $$PWD/authenticationdialog.ui

HEADERS += $$PWD/kdtoolsglobal.h \
    $$PWD/kdjob.h \
    $$PWD/kdgenericfactory.h \
    $$PWD/kdselfrestarter.h \
    $$PWD/kdrunoncechecker.h \
    $$PWD/kdlockfile.h \
    $$PWD/kdsysinfo.h

SOURCES += $$PWD/kdjob.cpp \
    $$PWD/kdselfrestarter.cpp \
    $$PWD/kdrunoncechecker.cpp \
    $$PWD/kdlockfile.cpp \
    $$PWD/kdsysinfo.cpp


HEADERS += $$PWD/kdupdater.h \
    $$PWD/kdupdaterfiledownloader.h \
    $$PWD/kdupdaterfiledownloader_p.h \
    $$PWD/kdupdaterfiledownloaderfactory.h \
    $$PWD/localpackagehub.h \
    $$PWD/kdupdaterupdate.h \
    $$PWD/kdupdaterupdateoperation.h \
    $$PWD/kdupdaterupdateoperationfactory.h \
    $$PWD/kdupdaterupdateoperations.h \
    $$PWD/kdupdatertask.h \
    $$PWD/kdupdaterupdatefinder.h \
    $$PWD/kdupdaterupdatesinfo_p.h \
    $$PWD/environment.h \
    $$PWD/kdupdaterupdatesinfodata_p.h

SOURCES += $$PWD/kdupdaterfiledownloader.cpp \
    $$PWD/kdupdaterfiledownloaderfactory.cpp \
    $$PWD/localpackagehub.cpp \
    $$PWD/kdupdaterupdate.cpp \
    $$PWD/kdupdaterupdateoperation.cpp \
    $$PWD/kdupdaterupdateoperationfactory.cpp \
    $$PWD/kdupdaterupdateoperations.cpp \
    $$PWD/kdupdatertask.cpp \
    $$PWD/kdupdaterupdatefinder.cpp \
    $$PWD/kdupdaterupdatesinfo.cpp \
    $$PWD/environment.cpp

unix:SOURCES += $$PWD/kdlockfile_unix.cpp
win32:SOURCES += $$PWD/kdlockfile_win.cpp
win32:SOURCES += $$PWD/kdsysinfo_win.cpp
macx:SOURCES += $$PWD/kdsysinfo_mac.cpp
unix:!macx:SOURCES += $$PWD/kdsysinfo_x11.cpp
