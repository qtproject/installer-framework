TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS += libinstaller installerbase binarycreator repogen archivegen tests

test.target = test
test.depends = $(TARGET)
test.commands = (cd tests && $(MAKE) test)

QMAKE_EXTRA_TARGETS += test

TRANSLATIONS += installerbase/translations/de_de.ts \
    installerbase/translations/sv_se.ts
