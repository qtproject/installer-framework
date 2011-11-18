DEPENDPATH += $$PWD
INCLUDEPATH += $$PWD

CONFIG(shared, static|shared) {
    DEFINES += BUILD_SHARED_KDTOOLSCORE
    DEFINES += BUILD_SHARED_KDUPDATER
}

HEADERS += $$PWD/kdtoolsglobal.h \
    $$PWD/kdjob.h \
    $$PWD/kdgenericfactory.h \
    $$PWD/kdselfrestarter.h \
    $$PWD/kdsavefile.h \
    $$PWD/kdrunoncechecker.h \
    $$PWD/kdlockfile.h \
    $$PWD/kdsysinfo.h

SOURCES += $$PWD/kdjob.cpp \
    $$PWD/kdgenericfactory.cpp \
    $$PWD/kdselfrestarter.cpp \
    $$PWD/kdsavefile.cpp \
    $$PWD/kdrunoncechecker.cpp \
    $$PWD/kdlockfile.cpp \
    $$PWD/kdsysinfo.cpp


HEADERS += $$PWD/kdupdater.h \
    $$PWD/kdupdaterapplication.h \
    $$PWD/kdupdatercrypto.h \
    $$PWD/kdupdaterfiledownloader.h \
    $$PWD/kdupdaterfiledownloader_p.h \
    $$PWD/kdupdaterfiledownloaderfactory.h \
    $$PWD/kdupdaterpackagesinfo.h \
    $$PWD/kdupdatersignatureverificationresult.h \
    $$PWD/kdupdatersignatureverifier.h \
    $$PWD/kdupdaterupdate.h \
    $$PWD/kdupdaterupdateoperation.h \
    $$PWD/kdupdaterupdateoperationfactory.h \
    $$PWD/kdupdaterupdateoperations.h \
    $$PWD/kdupdaterupdatesourcesinfo.h \
    $$PWD/kdupdatertask.h \
    $$PWD/kdupdatersignatureverificationrunnable.h \
    $$PWD/kdupdaterupdatefinder.h \
    $$PWD/kdupdaterupdatesinfo_p.h \
    $$PWD/kdupdaterupdateinstaller.h \
    $$PWD/kdupdaterufuncompressor_p.h \
    $$PWD/kdupdaterufcompresscommon_p.h \
    $$PWD/environment.h

SOURCES += $$PWD/kdupdaterapplication.cpp \
    $$PWD/kdupdatercrypto.cpp \
    $$PWD/kdupdaterfiledownloader.cpp \
    $$PWD/kdupdaterfiledownloaderfactory.cpp \
    $$PWD/kdupdaterpackagesinfo.cpp \
    $$PWD/kdupdatersignatureverificationresult.cpp \
    $$PWD/kdupdatersignatureverifier.cpp \
    $$PWD/kdupdaterupdate.cpp \
    $$PWD/kdupdaterupdateoperation.cpp \
    $$PWD/kdupdaterupdateoperationfactory.cpp \
    $$PWD/kdupdaterupdateoperations.cpp \
    $$PWD/kdupdaterupdatesourcesinfo.cpp \
    $$PWD/kdupdatertask.cpp \
    $$PWD/kdupdatersignatureverificationrunnable.cpp \
    $$PWD/kdupdaterupdatefinder.cpp \
    $$PWD/kdupdaterupdatesinfo.cpp \
    $$PWD/kdupdaterupdateinstaller.cpp \
    $$PWD/kdupdaterufuncompressor.cpp \
    $$PWD/kdupdaterufcompresscommon.cpp \
    $$PWD/environment.cpp

QT += gui

unix:SOURCES += $$PWD/kdlockfile_unix.cpp
win32:SOURCES += $$PWD/kdlockfile_win.cpp
win32:SOURCES += $$PWD/kdsysinfo_win.cpp
macx:SOURCES += $$PWD/kdsysinfo_mac.cpp
unix:!macx:SOURCES += $$PWD/kdsysinfo_x11.cpp


TRY_INCLUDEPATHS = /include /usr/include /usr/local/include $$QMAKE_INCDIR $$INCLUDEPATH
win32:TRY_INCLUDEPATHS += $$PWD/../openssl-0.9.8k/src/include
linux-lsb-g++:TRY_INCLUDEPATHS = $$QMAKE_INCDIR $$INCLUDEPATH
for(p, TRY_INCLUDEPATHS) {
    pp = $$join(p, "", "", "/openssl")
    exists($$pp):INCLUDEPATH *= $$p
}
