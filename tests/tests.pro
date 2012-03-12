TEMPLATE = subdirs

DESTDIR = bin
SUBDIRS += extractarchiveoperationtest environmentvariable fileengineclient fileengineserver

unix:test.commands = ./bin/extractarchiveoperationtest
win32:test.commands = bin\\extractarchiveoperationtest.exe
