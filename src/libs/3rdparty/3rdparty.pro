TEMPLATE = subdirs

include(../../../installerfw.pri)

CONFIG(libarchive) {
    SUBDIRS += libarchive
}
CONFIG(lzmasdk) {
    SUBDIRS += 7zip
}
