TEMPLATE = lib
TARGET = installer
INCLUDEPATH += . ..

CONFIG += staticlib

include(../7zip/7zip.pri)
include(../kdtools/kdtools.pri)
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
win32:QT += winextras

HEADERS += packagemanagercore.h \
    packagemanagercore_p.h \
    packagemanagergui.h \
    binaryformat.h \
    binaryformatengine.h \
    binaryformatenginehandler.h \
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
    postoperation.h \
    settings.h \
    permissionsettings.h \
    downloadarchivesjob.h \
    init.h \
    adminauthorization.h \
    elevatedexecuteoperation.h \
    fakestopprocessforupdateoperation.h \
    lazyplaintextedit.h \
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
    lib7z_facade.h \
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
    unziptask.h \
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
    lib7z_guid.h \
    lib7z_create.h \
    lib7z_extract.h \
    lib7z_list.h \
    repositorycategory.h \
    componentselectionpage_p.h

SOURCES += packagemanagercore.cpp \
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
    postoperation.cpp \
    downloadarchivesjob.cpp \
    init.cpp \
    elevatedexecuteoperation.cpp \
    fakestopprocessforupdateoperation.cpp \
    lazyplaintextedit.cpp \
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
    lib7z_facade.cpp \
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
    unziptask.cpp \
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
    componentselectionpage_p.cpp

FORMS += proxycredentialsdialog.ui \
    serverauthenticationdialog.ui

RESOURCES += resources/installer.qrc

unix {
    osx: SOURCES += adminauthorization_mac.cpp
    else: SOURCES += adminauthorization_x11.cpp
}

LIBS += -l7z
win32 {
    SOURCES += adminauthorization_win.cpp sysinfo_win.cpp

    LIBS += -loleaut32 -luser32     # 7zip
    LIBS += -ladvapi32 -lpsapi      # kdtools
    LIBS += -lole32 -lshell32       # createshortcutoperation

    win32-g++*:LIBS += -lmpr -luuid
    win32-g++*:QMAKE_CXXFLAGS += -Wno-missing-field-initializers
}

target.path = $$[QT_INSTALL_LIBS]
INSTALLS += target
