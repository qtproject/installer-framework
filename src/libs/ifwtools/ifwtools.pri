INCLUDEPATH += $$PWD $$PWD/rcc

DEFINES += BUILD_SHARED_IFWTOOLS

HEADERS += $$PWD/rcc/rcc.h \
    $$PWD/rcc/qcorecmdlineargs_p.h

SOURCES += $$PWD/rcc/rcc.cpp \
    $$PWD/rcc/rccmain.cpp

HEADERS += $$PWD/ifwtools_global.h \
    $$PWD/repositorygen.h \
    $$PWD/binarycreator.h

SOURCES += $$PWD/repositorygen.cpp \
    $$PWD/binarycreator.cpp

RESOURCES += $$PWD/resources/ifwtools.qrc

