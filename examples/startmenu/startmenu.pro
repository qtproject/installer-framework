TEMPLATE = aux

OTHER_FILES = README

example.target = build_example
example.commands = ../../bin/binarycreator -c $$PWD/config/config.xml -p $$PWD/packages installer
QMAKE_EXTRA_TARGETS += example

default_target.target = first
default_target.depends = example
QMAKE_EXTRA_TARGETS += default_target
