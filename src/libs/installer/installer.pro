TEMPLATE = lib
TARGET = installer
DEPENDPATH += . ..
INCLUDEPATH += . ..

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
    include($$PRODUCTKEYCHECK_PRI_FILE)
} else {
    SOURCES += productkeycheck.cpp
}

DESTDIR = $$IFW_LIB_PATH
DLLDESTDIR = $$IFW_APP_PATH

DEFINES += BUILD_LIB_INSTALLER

QT += script \
    network \
    xml

HEADERS += packagemanagercore.h \
    packagemanagercore_p.h \
    packagemanagergui.h \
    binaryformat.h \
    binaryformatengine.h \
    binaryformatenginehandler.h \
    repository.h \
    zipjob.h \
    utils.h \
    errors.h \
    component.h \
    componentmodel.h \
    qinstallerglobal.h \
    qtpatch.h \
    persistentsettings.h \
    projectexplorer_export.h \
    qtpatchoperation.h \
    setpathonqtcoreoperation.h \
    setdemospathonqtoperation.h \
    setexamplespathonqtoperation.h \
    setpluginpathonqtcoreoperation.h \
    setimportspathonqtcoreoperation.h \
    replaceoperation.h \
    linereplaceoperation.h \
    registerdocumentationoperation.h \
    registerqtoperation.h  \
    registertoolchainoperation.h  \
    registerqtv2operation.h  \
    registerqtv23operation.h  \
    registerqtvqnxoperation.h  \
    setqtcreatorvalueoperation.h \
    addqtcreatorarrayvalueoperation.h \
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
    getrepositorymetainfojob.h \
    downloadarchivesjob.h \
    init.h \
    updater.h \
    operationrunner.h \
    updatesettings.h \
    adminauthorization.h \
    fsengineclient.h \
    fsengineserver.h \
    elevatedexecuteoperation.h \
    fakestopprocessforupdateoperation.h \
    lazyplaintextedit.h \
    progresscoordinator.h \
    minimumprogressoperation.h \
    performinstallationform.h \
    messageboxhandler.h \
    getrepositoriesmetainfojob.h \
    licenseoperation.h \
    component_p.h \
    qtcreator_constants.h \
    qtcreatorpersistentsettings.h \
    registerdefaultdebuggeroperation.h \
    updatecreatorsettingsfrom21to22operation.h \
    qprocesswrapper.h \
    qsettingswrapper.h \
    constants.h \
    packagemanagerproxyfactory.h \
    createlocalrepositoryoperation.h \
    lib7z_facade.h \
    link.h \
    createlinkoperation.h \
    applyproductkeyoperation.h

SOURCES += packagemanagercore.cpp \
    packagemanagercore_p.cpp \
    packagemanagergui.cpp \
    binaryformat.cpp \
    binaryformatengine.cpp \
    binaryformatenginehandler.cpp \
    repository.cpp \
    zipjob.cpp \
    fileutils.cpp \
    utils.cpp \
    component.cpp \
    componentmodel.cpp \
    qtpatch.cpp \
    persistentsettings.cpp \
    qtpatchoperation.cpp  \
    setpathonqtcoreoperation.cpp \
    setdemospathonqtoperation.cpp \
    setexamplespathonqtoperation.cpp \
    setpluginpathonqtcoreoperation.cpp \
    setimportspathonqtcoreoperation.cpp \
    replaceoperation.cpp \
    linereplaceoperation.cpp \
    registerdocumentationoperation.cpp \
    registerqtoperation.cpp \
    registertoolchainoperation.cpp  \
    registerqtv2operation.cpp \
    registerqtv23operation.cpp \
    registerqtvqnxoperation.cpp  \
    setqtcreatorvalueoperation.cpp \
    addqtcreatorarrayvalueoperation.cpp \
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
    getrepositorymetainfojob.cpp \
    downloadarchivesjob.cpp \
    init.cpp \
    updater.cpp \
    operationrunner.cpp \
    updatesettings.cpp \
    adminauthorization.cpp \
    fsengineclient.cpp \
    fsengineserver.cpp \
    elevatedexecuteoperation.cpp \
    fakestopprocessforupdateoperation.cpp \
    lazyplaintextedit.cpp \
    progresscoordinator.cpp \
    minimumprogressoperation.cpp \
    performinstallationform.cpp \
    messageboxhandler.cpp \
    getrepositoriesmetainfojob.cpp \
    licenseoperation.cpp \
    component_p.cpp \
    qtcreatorpersistentsettings.cpp \
    registerdefaultdebuggeroperation.cpp \
    updatecreatorsettingsfrom21to22operation.cpp \
    qprocesswrapper.cpp \
    templates.cpp \
    qsettingswrapper.cpp \
    settings.cpp \
    packagemanagerproxyfactory.cpp \
    createlocalrepositoryoperation.cpp \
    lib7z_facade.cpp \
    link.cpp \
    createlinkoperation.cpp \
    applyproductkeyoperation.cpp

RESOURCES += resources/patch_file_lists.qrc

macx {
    HEADERS += macrelocateqt.h \
               macreplaceinstallnamesoperation.h
    SOURCES += adminauthorization_mac.cpp \
               macrelocateqt.cpp \
               macreplaceinstallnamesoperation.cpp
}

unix:!macx:SOURCES += adminauthorization_x11.cpp

win32 {
    SOURCES += adminauthorization_win.cpp
    LIBS += -loleaut32 -lUser32     # 7zip
    LIBS += advapi32.lib psapi.lib  # kdtools
    LIBS += -lole32 # createshortcutoperation
    CONFIG(shared, static|shared):LIBS += -lshell32
}
