CONFIG += ordered
TEMPLATE = subdirs

SUBDIRS += \
    archivegen \
    binarycreator \
    repogen

EXTRASUBDIRS = \
    extractbinarydata \
    repocompare \
    repogenfromonlinerepo


include(../installerfw.pri)

!isEqual(IFW_SOURCE_TREE, $$IFW_BUILD_TREE) {
    for(SUBDIR, EXTRASUBDIRS) {
        mkdir.commands += $$QMAKE_MKDIR $$SUBDIR $${IFW_NEWLINE}
    }
    QMAKE_EXTRA_TARGETS += mkdir
}

for(SUBDIR, EXTRASUBDIRS) {
    tools.commands += cd $$SUBDIR && $(QMAKE) -r $$PWD/$$SUBDIR && $(MAKE) $${IFW_NEWLINE}
}
!isEqual(IFW_SOURCE_TREE, $$IFW_BUILD_TREE) {
    tools.depends = mkdir
}
QMAKE_EXTRA_TARGETS += tools
