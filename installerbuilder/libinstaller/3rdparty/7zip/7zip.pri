7ZIP_BASE=$$PWD

SOURCES += $$7ZIP_BASE/lib7z_facade.cpp
HEADERS += $$7ZIP_BASE/lib7z_facade.h

INCLUDEPATH += $$7ZIP_BASE

unix:include($$7ZIP_BASE/unix/unix.pri) #this is p7zip
win32:include($$7ZIP_BASE/win/win.pri) #this is 7zip
