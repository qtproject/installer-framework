CONFIG += ordered
TEMPLATE = subdirs

EXTRASUBDIRS = testapp


include(../installerfw.pri)

!isEqual(IFW_SOURCE_TREE, $$IFW_BUILD_TREE) {
    for(SUBDIR, EXTRASUBDIRS) {
        mkdir.commands += $$QMAKE_MKDIR $$SUBDIR $${IFW_NEWLINE}
    }
    QMAKE_EXTRA_TARGETS += mkdir
}

for(SUBDIR, EXTRASUBDIRS) {
    examples.commands += cd $$SUBDIR && $(QMAKE) -r $$PWD/$$SUBDIR && $(MAKE) $${IFW_NEWLINE}
}
!isEqual(IFW_SOURCE_TREE, $$IFW_BUILD_TREE) {
    examples.depends = mkdir
}
QMAKE_EXTRA_TARGETS += examples
