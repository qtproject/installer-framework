7ZIP_BASE=$$PWD

win32:include($$7ZIP_BASE/win/win.pri) #this is 7zip
unix:include($$7ZIP_BASE/unix/unix.pri) #this is p7zip
