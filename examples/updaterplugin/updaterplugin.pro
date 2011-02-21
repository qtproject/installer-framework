TEMPLATE = lib
TARGET = Updater

include( ../../installerbuilder/libinstaller/libinstaller.pri )

# maybe wo should not build a static plugin for Qt Creator
CONFIG -= static
CONFIG -= staticlib
CONFIG += shared

#PROVIDER = KDAB
IDE_PLUGIN_PATH = PlugInst

IDE_BUILD_TREE = "$$(QTCREATOR_SOURCE_PATH)"
include($$(QTCREATOR_SOURCE_PATH)/src/qtcreatorplugin.pri)
include($$(QTCREATOR_SOURCE_PATH)/src/libs/extensionsystem/extensionsystem.pri)
include($$(QTCREATOR_SOURCE_PATH)/src/plugins/coreplugin/coreplugin.pri)

QT += gui

CONFIG += uitools

LIBS = -L../../installerbuilder/lib -linstaller $$LIBS

DEST=$$DESTDIR/libUpdater.dylib
# this will make sure we are using the Qt from the QtCreater.app bundle
ddlib="$"$lib
macx:QMAKE_POST_LINK = for lib in `otool -L \"$${DEST}\" | sed -ne \'s,.*\\(libQt[^ ]*\\).*,\1,p\'`; do install_name_tool -change $$[QT_INSTALL_LIBS]/$${ddlib} @executable_path/../Frameworks/$${ddlib} \"$${DEST}\"; done;
macx:QMAKE_POST_LINK += ../../installerbuilder/bin/binarycreator -p packages -c config -t ../../installerbuilder/bin/installerbase -e com.nokia.qtcreator QtCreatorInstaller.app com.nokia.qtcreator ;

SOURCES += updaterplugin.cpp updatersettingspage.cpp

HEADERS += updaterplugin.h updatersettingspage.h

RESOURCES += updaterplugin.qrc

OTHER_FILES += Updater.pluginspec
