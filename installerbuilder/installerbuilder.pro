TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS += libinstaller installerbase binarycreator repogen archivegen tests

test.commands=(cd tests && $(MAKE) test)

test.target=test
test.depends = $(TARGET)
QMAKE_EXTRA_TARGETS += test

