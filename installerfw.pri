!isEmpty(IFW_PRI_INCLUDED) {
    error("installerfw.pri already included")
}
IFW_PRI_INCLUDED = 1

defineReplace(toNativeSeparators) {
    return($$replace(1, /, $$QMAKE_DIR_SEP))
}

defineReplace(cleanPath) {
    win32:1 ~= s|\\\\|/|g
    contains(1, ^/.*):pfx = /
    else:pfx =
    segs = $$split(1, /)
    out =
    for(seg, segs) {
        equals(seg, ..):out = $$member(out, 0, -2)
        else:!equals(seg, .):out += $$seg
    }
    return($$join(out, /, $$pfx))
}

isEmpty(IFW_BUILD_TREE) {
    sub_dir = $$_PRO_FILE_PWD_
    sub_dir ~= s,^$$re_escape($$PWD),,
    IFW_BUILD_TREE = $$cleanPath($$OUT_PWD)
    IFW_BUILD_TREE ~= s,$$re_escape($$sub_dir)$,,
}

IFW_SOURCE_TREE = $$PWD
IFW_APP_PATH = $$IFW_BUILD_TREE/bin
IFW_LIB_PATH = $$IFW_BUILD_TREE/lib

RCC = $$cleanPath($$toNativeSeparators($$[QT_INSTALL_BINS]/rcc))
LRELEASE = $$cleanPath($$toNativeSeparators($$[QT_INSTALL_BINS]/lrelease))

win32:RCC = $${RCC}.exe
win32:LRELEASE = $${LRELEASE}.exe

INCLUDEPATH += \
    $$IFW_SOURCE_TREE/src/libs/7zip \
    $$IFW_SOURCE_TREE/src/libs/kdtools \
    $$IFW_SOURCE_TREE/src/libs/installer
win32:INCLUDEPATH += $$IFW_SOURCE_TREE/src/libs/7zip/win/CPP
unix:INCLUDEPATH += $$IFW_SOURCE_TREE/src/libs/7zip/unix/CPP

LIBS += -L$$IFW_LIB_PATH
unix:!macx:LIBS += -lutil
macx:LIBS += -framework Carbon -framework Security

CONFIG += help uitools
CONFIG(static, static|shared) {
    QTPLUGIN += qsqlite
    QT += script network xml
    DEFINES += USE_STATIC_SQLITE_PLUGIN
}

GIT_SHA1 = $$system(git rev-list --abbrev-commit -n1 HEAD)
DEFINES += QT_NO_CAST_FROM_ASCII "_GIT_SHA1_=$$GIT_SHA1"

CONFIG(shared, static|shared):DEFINES += KDTOOLS_SHARED
CONFIG(shared, static|shared):DEFINES += LIB_INSTALLER_SHARED

static {
    win32:exists($$IFW_LIB_PATH/installer.lib):POST_TARGETDEPS += $$IFW_LIB_PATH/installer.lib
    unix:exists($$IFW_LIB_PATH/libinstaller.a):POST_TARGETDEPS += $$IFW_LIB_PATH/libinstaller.a
}
