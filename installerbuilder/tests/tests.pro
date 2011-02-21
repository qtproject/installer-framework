TEMPLATE=subdirs

DESTDIR = bin
SUBDIRS += kd7zenginetest extractarchiveoperationtest environmentvariable

unix:test.commands=./bin/kd7zenginetest && ./bin/extractarchiveoperationtest
win32:test.commands=bin\kd7zenginetest.exe && bin\extractarchiveoperationtest.exe

test.target=test
test.depends = $(TARGET)
QMAKE_EXTRA_TARGETS += test
