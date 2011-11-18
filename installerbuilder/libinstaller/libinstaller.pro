TEMPLATE = lib
TARGET = installer
DEPENDPATH += . \
    .. \
    ../common \
    3rdparty/kdtools 

INCLUDEPATH += . \
    .. \
    3rdparty/kdtools

DESTDIR = $$OUT_PWD/../lib
DLLDESTDIR = $$OUT_PWD/../bin

DEFINES += QT_NO_CAST_FROM_ASCII \
    BUILD_LIB_INSTALLER

CONFIG( shared, static|shared ){
    DEFINES += KDTOOLS_SHARED
}

QT += script \
    network \
    sql
CONFIG += help uitools

contains(CONFIG, static): {
    QTPLUGIN += qsqlite
    DEFINES += USE_STATIC_SQLITE_PLUGIN
}

include(3rdparty/p7zip_9.04/p7zip.pri)
include(3rdparty/kdtools/kdtools.pri)

HEADERS += $$PWD/packagemanagercore.h \
    $$PWD/packagemanagercore_p.h \
    $$PWD/packagemanagergui.h \
    ../common/binaryformat.h \
    ../common/binaryformatengine.h \
    ../common/binaryformatenginehandler.h \
    ../common/repository.h \
    ../common/zipjob.h \
    ../common/utils.h \
    ../common/errors.h \
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
    constants.h

SOURCES += $$PWD/packagemanagercore.cpp \
    $$PWD/packagemanagercore_p.cpp \
    $$PWD/packagemanagergui.cpp \
    ../common/binaryformat.cpp \
    ../common/binaryformatengine.cpp \
    ../common/binaryformatenginehandler.cpp \
    ../common/repository.cpp \
    ../common/zipjob.cpp \
    ../common/fileutils.cpp \
    ../common/utils.cpp \
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
    settings.cpp

macx {
    HEADERS +=  macrelocateqt.h \
                macreplaceinstallnamesoperation.h
    SOURCES +=  macrelocateqt.cpp \
                macreplaceinstallnamesoperation.cpp
}

win32:SOURCES += adminauthorization_win.cpp
macx:SOURCES += adminauthorization_mac.cpp
unix:!macx: SOURCES += adminauthorization_x11.cpp

win32:OBJECTS_DIR = .obj
win32:LIBS += ole32.lib \
    oleaut32.lib \
    user32.lib

# Needed by kdtools (in kdlog_win.cpp):
win32:LIBS += advapi32.lib psapi.lib
macx:LIBS += -framework Carbon

CONFIG( shared, static|shared ): {
  DEFINES += LIB_INSTALLER_SHARED
  win32: LIBS += shell32.lib
}

macx: LIBS += -framework Security

TRANSLATIONS += translations/de_de.ts \
    translations/sv_se.ts

RESOURCES += ../common/openssl.qrc \
    resources/patch_file_lists.qrc
