TEMPLATE=subdirs
CONFIG += ordered
SUBDIRS += installerbuilder tools examples


test.target=test
test.commands=(cd installerbuilder && $(MAKE) test)
test.depends = $(TARGET)
QMAKE_EXTRA_TARGETS += test

