!isEmpty(IFW_PRI_INCLUDED) {
    error("installerfw.pri already included")
}
IFW_PRI_INCLUDED = 1

IFW_VERSION_STR = 4.7.0
IFW_VERSION = 0x040700
IFW_VERSION_WIN32 = 4,7,0,0

IFW_VERSION_STR_WIN32 = $$IFW_VERSION_STR\0

IFW_REPOSITORY_FORMAT_VERSION = 1.0.0
IFW_CACHE_FORMAT_VERSION = 1.1.0
IFW_NEWLINE = $$escape_expand(\\n\\t)

isEmpty(IFW_DISABLE_TRANSLATIONS): IFW_DISABLE_TRANSLATIONS = $$(IFW_DISABLE_TRANSLATIONS)
isEqual(IFW_DISABLE_TRANSLATIONS, 1) {
    DEFINES += IFW_DISABLE_TRANSLATIONS
}

# Still default to LZMA SDK if nothing is defined by user
!contains(CONFIG, libarchive|lzmasdk): CONFIG += lzmasdk

CONFIG(lzmasdk) {
    LZMA_WARNING_MSG = "LZMA SDK as an archive handler in IFW is deprecated. Consider" \
        "switching to libarchive by appending 'libarchive' to your 'CONFIG' variable." \
        "This requires linking against additional external libraries, see the" \
        "'INSTALL' file for more details."
    !build_pass:warning($$LZMA_WARNING_MSG)
}

defineTest(minQtVersion) {
    maj = $$1
    min = $$2
    patch = $$3
    isEqual(QT_MAJOR_VERSION, $$maj) {
        isEqual(QT_MINOR_VERSION, $$min) {
            isEqual(QT_PATCH_VERSION, $$patch) {
                return(true)
            }
            greaterThan(QT_PATCH_VERSION, $$patch) {
                return(true)
            }
        }
        greaterThan(QT_MINOR_VERSION, $$min) {
            return(true)
        }
    }
    greaterThan(QT_MAJOR_VERSION, $$maj) {
        return(true)
    }
    return(false)
}

defineReplace(toNativeSeparators) {
    return($$replace(1, /, $$QMAKE_DIR_SEP))
}
defineReplace(fromNativeSeparators) {
    return($$replace(1, \\\\, /))
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
isEmpty(IFW_LIB_PATH) {
    IFW_LIB_PATH = $$IFW_BUILD_TREE/lib
}

RCC = $$toNativeSeparators($$cleanPath($$[QT_INSTALL_BINS]/rcc))
LRELEASE = $$toNativeSeparators($$cleanPath($$[QT_INSTALL_BINS]/lrelease))
LUPDATE = $$toNativeSeparators($$cleanPath($$[QT_INSTALL_BINS]/lupdate))
LCONVERT = $$toNativeSeparators($$cleanPath($$[QT_INSTALL_BINS]/lconvert))
QMAKE_BINARY = $$toNativeSeparators($$cleanPath($$[QT_INSTALL_BINS]/qmake))

win32 {
    RCC = $${RCC}.exe
    LRELEASE = $${LRELEASE}.exe
    LUPDATE = $${LUPDATE}.exe
    LCONVERT = $${LCONVERT}.exe
    QMAKE_BINARY = $${QMAKE_BINARY}.exe
}
win32-g++*:QMAKE_CXXFLAGS += -Wno-attributes
macx:QMAKE_CXXFLAGS += -fvisibility=hidden -fvisibility-inlines-hidden

INCLUDEPATH += \
    $$IFW_SOURCE_TREE/src/libs/kdtools \
    $$IFW_SOURCE_TREE/src/libs/ifwtools \
    $$IFW_SOURCE_TREE/src/libs/installer

CONFIG(libarchive): INCLUDEPATH += $$IFW_SOURCE_TREE/src/libs/3rdparty/libarchive

CONFIG(lzmasdk) {
    INCLUDEPATH += $$IFW_SOURCE_TREE/src/libs/3rdparty/7zip
    win32:INCLUDEPATH += $$IFW_SOURCE_TREE/src/libs/3rdparty/7zip/win/CPP \
        $$IFW_SOURCE_TREE/src/libs/3rdparty/7zip/win/C
    unix:INCLUDEPATH += $$IFW_SOURCE_TREE/src/libs/3rdparty/7zip/unix/CPP \
        $$IFW_SOURCE_TREE/src/libs/3rdparty/7zip/unix/C
}

LIBS += -L$$IFW_LIB_PATH
# The order is important. The linker needs to parse archives in reversed dependency order.
equals(TEMPLATE, app):LIBS += -linstaller
win32:equals(TEMPLATE, app) {
    LIBS += -luser32
}
unix:!macx:LIBS += -lutil
macx:LIBS += -framework Carbon -framework Security


#
# Qt 5 sets QT_CONFIG
# Use same static/shared configuration as Qt
#
!contains(CONFIG, static|shared) {
    contains(QT_CONFIG, static): CONFIG += static
    contains(QT_CONFIG, shared): CONFIG += shared
}

QT += uitools core-private
CONFIG(static, static|shared) {
    win32:lessThan(QT_MAJOR_VERSION, 6):QT += winextras
    QT += concurrent network qml xml
    greaterThan(QT_MAJOR_VERSION, 5):QT += core5compat
}
CONFIG += depend_includepath no_private_qt_headers_warning
versionAtLeast(QT_MAJOR_VERSION, 6) {
    CONFIG+=c++17
} else {
    CONFIG+=c++11
}

win32:CONFIG += console

exists(".git") {
    GIT_SHA1 = $$system(git rev-list --abbrev-commit -n1 HEAD)
}

isEmpty(GIT_SHA1) {
    # Attempt to read the sha1 from alternative location
    GIT_SHA1=\"$$cat(.tag)\"
}

DEFINES += NOMINMAX QT_NO_CAST_FROM_ASCII QT_STRICT_ITERATORS QT_USE_QSTRINGBUILDER \
           "_GIT_SHA1_=$$GIT_SHA1" \
           IFW_VERSION_STR=$$IFW_VERSION_STR \
           IFW_VERSION=$$IFW_VERSION \
           IFW_VERSION_STR_WIN32=$$IFW_VERSION_STR_WIN32 \
           IFW_VERSION_WIN32=$$IFW_VERSION_WIN32
DEFINES += IFW_REPOSITORY_FORMAT_VERSION=$$IFW_REPOSITORY_FORMAT_VERSION \
           IFW_CACHE_FORMAT_VERSION=$$IFW_CACHE_FORMAT_VERSION

win32-g++*: LIBS += -lmpr -luuid

equals(TEMPLATE, app) {
    msvc:POST_TARGETDEPS += $$IFW_LIB_PATH/installer.lib
    win32-g++*:POST_TARGETDEPS += $$IFW_LIB_PATH/libinstaller.a
    unix:POST_TARGETDEPS += $$IFW_LIB_PATH/libinstaller.a
}

CONFIG(libarchive):equals(TEMPLATE, app) {
    LIBS += -llibarchive
    !isEmpty(IFW_ZLIB_LIBRARY) {
        LIBS += $$IFW_ZLIB_LIBRARY
    } else:!contains(QT_MODULES, zlib) {
        unix:LIBS += -lz
        win32:LIBS += -lzlib
    }
    !isEmpty(IFW_BZIP2_LIBRARY) {
        LIBS += $$IFW_BZIP2_LIBRARY
    } else {
        unix:LIBS += -lbz2
        win32:LIBS += -llibbz2
    }
    !isEmpty(IFW_LZMA_LIBRARY) {
        LIBS += $$IFW_LZMA_LIBRARY
    } else {
        unix:LIBS += -llzma
        win32:LIBS += -lliblzma
    }
    macos {
        !isEmpty(IFW_ICONV_LIBRARY) {
            LIBS += $$IFW_ICONV_LIBRARY
        } else {
            LIBS += -liconv
        }
    }

    msvc:POST_TARGETDEPS += $$IFW_LIB_PATH/libarchive.lib
    win32-g++*:POST_TARGETDEPS += $$IFW_LIB_PATH/liblibarchive.a
    unix:POST_TARGETDEPS += $$IFW_LIB_PATH/liblibarchive.a
}

CONFIG(lzmasdk):equals(TEMPLATE, app) {
    LIBS += -l7z

    msvc:POST_TARGETDEPS += $$IFW_LIB_PATH/7z.lib
    win32-g++*:POST_TARGETDEPS += $$IFW_LIB_PATH/lib7z.a
    unix:POST_TARGETDEPS += $$IFW_LIB_PATH/lib7z.a
}
