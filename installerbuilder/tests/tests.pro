TEMPLATE=subdirs

DESTDIR = bin
SUBDIRS += extractarchiveoperationtest environmentvariable

unix:test.commands=./bin/extractarchiveoperationtest
win32:test.commands=bin\\extractarchiveoperationtest.exe

test.target=test
test.depends = $(TARGET)
QMAKE_EXTRA_TARGETS += test
