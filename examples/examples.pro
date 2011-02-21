TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS += testapp
SUBDIRS += updater

IDE_BUILD_TREE = "$$(QTCREATOR_SOURCE_PATH)"
!isEmpty( IDE_BUILD_TREE ):SUBDIRS += updaterplugin
