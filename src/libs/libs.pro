TEMPLATE = subdirs

include(../../installerfw.pri)

SUBDIRS += 3rdparty installer
installer.depends = 3rdparty

CONFIG(lzmasdk) {
    SUBDIRS += 7zip
    installer.depends = 7zip
}
