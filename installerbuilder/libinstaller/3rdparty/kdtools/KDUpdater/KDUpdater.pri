DEPENDPATH += $$PWD
INCLUDEPATH += $$PWD

CONFIG( shared, static|shared) {
    DEFINES += BUILD_SHARED_KDUPDATER
}

HEADERS += $$PWD/kdupdater.h \
    $$PWD/kdupdaterapplication.h \
    $$PWD/kdupdatercrypto.h \
    $$PWD/kdupdaterfiledownloader.h \
    $$PWD/kdupdaterfiledownloader_p.h \
    $$PWD/kdupdaterfiledownloaderfactory.h \
    $$PWD/kdupdaterpackagesinfo.h \
    $$PWD/kdupdaterpackagesview.h \
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
    $$PWD/kdupdaterupdatesdialog.h \
    $$PWD/kdupdaterupdatesourcesview.h \
    $$PWD/kdupdaterufcompresscommon_p.h \
    $$PWD/environment.h

SOURCES += $$PWD/kdupdaterapplication.cpp \
    $$PWD/kdupdatercrypto.cpp \
    $$PWD/kdupdaterfiledownloader.cpp \
    $$PWD/kdupdaterfiledownloaderfactory.cpp \
    $$PWD/kdupdaterpackagesinfo.cpp \
    $$PWD/kdupdaterpackagesview.cpp \
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
    $$PWD/kdupdaterupdatesdialog.cpp \
    $$PWD/kdupdaterupdatesourcesview.cpp \
    $$PWD/kdupdaterufcompresscommon.cpp \
    $$PWD/environment.cpp



FORMS += $$PWD/updatesdialog.ui \
    $$PWD/addupdatesourcedialog.ui

DEFINES += KDUPDATERGUITEXTBROWSER \
    KDUPDATERVIEW=QTextBrowser
QT += gui

TRY_INCLUDEPATHS = /include /usr/include /usr/local/include $$QMAKE_INCDIR $$INCLUDEPATH
win32:TRY_INCLUDEPATHS += $$PWD/../../3rdparty/openssl-0.9.8k/src/include
linux-lsb-g++:TRY_INCLUDEPATHS = $$QMAKE_INCDIR $$INCLUDEPATH
for(p, TRY_INCLUDEPATHS) {
    pp = $$join(p, "", "", "/openssl")
    exists($$pp):INCLUDEPATH *= $$p
}
