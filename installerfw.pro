CONFIG += ordered
TEMPLATE = subdirs
SUBDIRS += src tests tools

include (installerfw.pri)
include (doc/doc.pri)

!minQtVersion(5, 3, 0) {
    message("Cannot build Qt Installer Framework with Qt version $${QT_VERSION}.")
    error("Use at least Qt 5.3.0.")
}
