CONFIG += ordered
TEMPLATE = subdirs
SUBDIRS += src tools

include (installerfw.pri)
include (doc/doc.pri)

BUILD_TESTS = $$(BUILDTESTS)
isEmpty(BUILD_TESTS):BUILD_TESTS=$${BUILDTESTS}
!isEmpty(BUILD_TESTS):SUBDIRS += tests

BUILD_EXAMPLES = $$(BUILDEXAMPLES)
isEmpty(BUILD_EXAMPLES):BUILD_EXAMPLES=$${BUILDEXAMPLES}
!isEmpty(BUILD_EXAMPLES):SUBDIRS += examples

!minQtVersion(5, 5, 0) {
    message("Cannot build Qt Installer Framework with Qt version $${QT_VERSION}.")
    error("Use at least Qt 5.5.0.")
}
