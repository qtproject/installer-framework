TEMPLATE=subdirs
CONFIG += ordered
SUBDIRS += installerbuilder examples tools
mac:SUBDIRS -= examples

test.target = test
test.depends = $(TARGET)
test.commands = (cd installerbuilder && $(MAKE) test)

QMAKE_EXTRA_TARGETS += test

include (doc/doc.pri)
