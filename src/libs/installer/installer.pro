TEMPLATE = lib
TARGET = installer
INCLUDEPATH += . ..

CONFIG += staticlib

include(../kdtools/kdtools.pri)
include(../ifwtools/ifwtools.pri)
include(../../../installerfw.pri)

# productkeycheck API
# call qmake "PRODUCTKEYCHECK_PRI_FILE=<your_path_to_pri_file>"
# content of that pri file needs to be:
#   SOURCES += $$PWD/productkeycheck.cpp
#   ...
#   your files if needed
HEADERS += productkeycheck.h
!isEmpty(PRODUCTKEYCHECK_PRI_FILE) {
    # use undocumented no_batch config which disable the implicit rules on msvc compilers
    # this fixes the problem that same cpp files in different directories are overwritting
    # each other
    CONFIG += no_batch
    include($$PRODUCTKEYCHECK_PRI_FILE)
} else {
    SOURCES += productkeycheck.cpp
    SOURCES += commandlineparser_p.cpp
}

DESTDIR = $$IFW_LIB_PATH
DLLDESTDIR = $$IFW_APP_PATH

DEFINES += BUILD_LIB_INSTALLER

QT += \
    qml \
    network \
    xml \
    concurrent \
    widgets \
    core-private \
    qml-private

win32:lessThan(QT_MAJOR_VERSION, 6):QT += winextras

greaterThan(QT_MAJOR_VERSION, 5):QT += core5compat

HEADERS += packagemanagercore.h \
    aspectratiolabel.h \
    calculatorbase.h \
    componentalias.h \
    componentsortfilterproxymodel.h \
    concurrentoperationrunner.h \
    genericdatacache.h \
    loggingutils.h \
    metadata.h \
    metadatacache.h \
    packagemanagercore_p.h \
    packagemanagergui.h \
    binaryformat.h \
    binaryformatengine.h \
    binaryformatenginehandler.h \
    fileguard.h \
    repository.h \
    utils.h \
    errors.h \
    component.h \
    scriptengine.h \
    componentmodel.h \
    qinstallerglobal.h \
    qtpatch.h \
    consumeoutputoperation.h \
    replaceoperation.h \
    linereplaceoperation.h \
    copydirectoryoperation.h \
    simplemovefileoperation.h \
    extractarchiveoperation.h \
    extractarchiveoperation_p.h \
    globalsettingsoperation.h \
    createshortcutoperation.h \
    createdesktopentryoperation.h \
    registerfiletypeoperation.h \
    environmentvariablesoperation.h \
    installiconsoperation.h \
    selfrestartoperation.h \
    settings.h \
    permissionsettings.h \
    downloadarchivesjob.h \
    init.h \
    adminauthorization.h \
    elevatedexecuteoperation.h \
    fakestopprocessforupdateoperation.h \
    progresscoordinator.h \
    minimumprogressoperation.h \
    performinstallationform.h \
    messageboxhandler.h \
    licenseoperation.h \
    component_p.h \
    qprocesswrapper.h \
    qsettingswrapper.h \
    constants.h \
    packagemanagerproxyfactory.h \
    createlocalrepositoryoperation.h \
    link.h \
    createlinkoperation.h \
    packagemanagercoredata.h \
    globals.h \
    graph.h \
    settingsoperation.h \
    testrepository.h \
    packagemanagerpagefactory.h \
    abstracttask.h\
    abstractfiletask.h \
    copyfiletask.h \
    downloadfiletask.h \
    downloadfiletask_p.h \
    observer.h \
    runextensions.h \
    metadatajob.h \
    metadatajob_p.h \
    installer_global.h \
    scriptengine_p.h \
    protocol.h \
    remoteobject.h \
    remoteclient.h \
    remoteserver.h \
    remoteclient_p.h \
    remoteserver_p.h \
    remotefileengine.h \
    remoteserverconnection.h \
    remoteserverconnection_p.h \
    fileio.h \
    binarycontent.h \
    binarylayout.h \
    installercalculator.h \
    uninstallercalculator.h \
    componentchecker.h \
    proxycredentialsdialog.h \
    serverauthenticationdialog.h \
    keepaliveobject.h \
    systeminfo.h \
    packagesource.h \
    repositorycategory.h \
    componentselectionpage_p.h \
    commandlineparser.h \
    commandlineparser_p.h \
    abstractarchive.h \
    directoryguard.h \
    archivefactory.h \
    operationtracer.h \
    customcombobox.h

SOURCES += packagemanagercore.cpp \
    abstractarchive.cpp \
    archivefactory.cpp \
    aspectratiolabel.cpp \
    calculatorbase.cpp \
    componentalias.cpp \
    concurrentoperationrunner.cpp \
    directoryguard.cpp \
    fileguard.cpp \
    componentsortfilterproxymodel.cpp \
    genericdatacache.cpp \
    loggingutils.cpp \
    metadata.cpp \
    metadatacache.cpp \
    operationtracer.cpp \
    packagemanagercore_p.cpp \
    packagemanagergui.cpp \
    binaryformat.cpp \
    binaryformatengine.cpp \
    binaryformatenginehandler.cpp \
    repository.cpp \
    fileutils.cpp \
    utils.cpp \
    component.cpp \
    scriptengine.cpp \
    componentmodel.cpp \
    qtpatch.cpp \
    consumeoutputoperation.cpp \
    replaceoperation.cpp \
    linereplaceoperation.cpp \
    copydirectoryoperation.cpp \
    simplemovefileoperation.cpp \
    extractarchiveoperation.cpp \
    globalsettingsoperation.cpp \
    createshortcutoperation.cpp \
    createdesktopentryoperation.cpp \
    registerfiletypeoperation.cpp \
    environmentvariablesoperation.cpp \
    installiconsoperation.cpp \
    selfrestartoperation.cpp \
    downloadarchivesjob.cpp \
    init.cpp \
    elevatedexecuteoperation.cpp \
    fakestopprocessforupdateoperation.cpp \
    progresscoordinator.cpp \
    minimumprogressoperation.cpp \
    performinstallationform.cpp \
    messageboxhandler.cpp \
    licenseoperation.cpp \
    component_p.cpp \
    qprocesswrapper.cpp \
    qsettingswrapper.cpp \
    settings.cpp \
    permissionsettings.cpp \
    packagemanagerproxyfactory.cpp \
    createlocalrepositoryoperation.cpp \
    link.cpp \
    createlinkoperation.cpp \
    packagemanagercoredata.cpp \
    globals.cpp \
    settingsoperation.cpp \
    testrepository.cpp \
    packagemanagerpagefactory.cpp \
    abstractfiletask.cpp \
    copyfiletask.cpp \
    downloadfiletask.cpp \
    observer.cpp \
    metadatajob.cpp \
    protocol.cpp \
    remoteobject.cpp \
    remoteclient.cpp \
    remoteserver.cpp \
    remotefileengine.cpp \
    remoteserverconnection.cpp \
    fileio.cpp \
    binarycontent.cpp \
    binarylayout.cpp \
    installercalculator.cpp \
    uninstallercalculator.cpp \
    componentchecker.cpp \
    proxycredentialsdialog.cpp \
    serverauthenticationdialog.cpp \
    keepaliveobject.cpp \
    systeminfo.cpp \
    packagesource.cpp \
    repositorycategory.cpp \
    componentselectionpage_p.cpp \
    commandlineparser.cpp \
    customcombobox.cpp

macos:SOURCES += fileutils_mac.mm

FORMS += proxycredentialsdialog.ui \
    serverauthenticationdialog.ui

RESOURCES += resources/installer.qrc

unix {
    osx: SOURCES += adminauthorization_mac.cpp
    else: SOURCES += adminauthorization_x11.cpp
}

CONFIG(libarchive) {
    HEADERS += libarchivearchive.h \
        libarchivewrapper.h \
        libarchivewrapper_p.h

    SOURCES += libarchivearchive.cpp \
        libarchivewrapper.cpp \
        libarchivewrapper_p.cpp

    LIBS += -llibarchive
}

CONFIG(lzmasdk) {
    include(../3rdparty/7zip/7zip.pri)

    HEADERS += lib7z_facade.h \
        lib7z_guid.h \
        lib7z_create.h \
        lib7z_extract.h \
        lib7z_list.h \
        lib7zarchive.h

    SOURCES += lib7z_facade.cpp \
        lib7zarchive.cpp

    LIBS += -l7z
    win32:LIBS += -loleaut32 -luser32
}

win32 {
    SOURCES += adminauthorization_win.cpp sysinfo_win.cpp

    LIBS += -ladvapi32 -lpsapi      # kdtools
    LIBS += -lole32 -lshell32       # createshortcutoperation

    win32-g++*:LIBS += -lmpr -luuid
    win32-g++*:QMAKE_CXXFLAGS += -Wno-missing-field-initializers
}

target.path = $$[QT_INSTALL_LIBS]
INSTALLS += target
