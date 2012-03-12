CONFIG += ordered
TEMPLATE = subdirs
!macx:SUBDIRS += examples
SUBDIRS += installerbuilder tools

test.target = test
test.depends = $(TARGET)
QMAKE_EXTRA_TARGETS += test
test.commands = (cd tests && $(QMAKE) && $(MAKE))

include (doc/doc.pri)
