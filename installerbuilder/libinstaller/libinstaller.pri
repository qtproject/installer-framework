macx:DEFINES += _LZMA_UINT32_IS_ULONG

DEFINES += FSENGINE_TCP

INCLUDEPATH += $$PWD \
    $$PWD/.. \
    $$PWD/kdtools

DEPENDPATH += $$PWD \
    $$PWD/.. \
    $$PWD/kdtools/KDToolsCore \
    $$PWD/kdtools/KDUpdater


INCLUDEPATH += $$PWD/3rdparty/p7zip_9.04/unix/CPP \
               $$PWD/3rdparty/p7zip_9.04

DEPENDPATH += $$PWD/3rdparty/p7zip_9.04/unix/CPP \
              $$PWD/3rdparty/p7zip_9.04

CONFIG( shared, static|shared ):DEFINES += LIB_INSTALLER_SHARED
CONFIG( shared, static|shared ):DEFINES += KDTOOLS_SHARED

CONFIG += uitools help
QTPLUGIN += qsqlite

QT += script
QT += gui # gui needed for KDUpdater include (compareVersion), which indirectly include QTreeWidget

LIBS = -L$$OUT_PWD/../lib -L$$OUT_PWD/../../lib -linstaller $$LIBS

win32:LIBS += -lole32 -lUser32 -loleaut32 -lshell32
macx:LIBS += -framework Security
unix:!macx:LIBS += -lutil

static {
    unix {
        exists($$OUT_PWD/../lib/libinstaller.a):POST_TARGETDEPS += $$OUT_PWD/../lib/libinstaller.a
        exists($$OUT_PWD/../../lib/libinstaller.a):POST_TARGETDEPS += $$OUT_PWD/../../lib/libinstaller.a
    }
    win32 {
        exists($$OUT_PWD/../lib/installer.lib):POST_TARGETDEPS += $$OUT_PWD/../lib/installer.lib
        exists($$OUT_PWD/../../lib/installer.lib):POST_TARGETDEPS += $$OUT_PWD/../../lib/installer.lib
    }
}
