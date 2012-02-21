INCLUDEPATH += $$PWD \
    $$PWD/libinstaller \
    $$PWD/libinstaller/3rdparty/kdtools \
    $$PWD/libinstaller/3rdparty/p7zip_9.04 \
    $$PWD/libinstaller/3rdparty/p7zip_9.04/unix/CPP

DEPENDPATH += $$PWD \
    $$PWD/libinstaller \
    $$PWD/libinstaller/3rdparty/p7zip_9.04 \
    $$PWD/libinstaller/3rdparty/p7zip_9.04/unix/CPP \
    $$PWD/libinstaller/3rdparty/kdtools

QT += script
CONFIG += uitools help

DESTDIR = $$PWD/bin
LIBS = -L$$PWD/lib -linstaller $$LIBS

contains(CONFIG, static): {
    QTPLUGIN += qsqlite
    DEFINES += USE_STATIC_SQLITE_PLUGIN
}

CONFIG(shared, static|shared): {
    DEFINES += LIB_INSTALLER_SHARED KDTOOLS_SHARED
}

unix:!macx:LIBS += -lutil
macx:LIBS += -framework Security
macx:DEFINES += _LZMA_UINT32_IS_ULONG
win32:LIBS += -lole32 -lUser32 -loleaut32 -lshell32

static {
    unix {
        exists($$PWD/lib/libinstaller.a):POST_TARGETDEPS += $$PWD/lib/libinstaller.a
    }
    win32 {
        exists($$PWD/lib/installer.lib):POST_TARGETDEPS += $$PWD/lib/installer.lib
    }
}

