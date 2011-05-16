TEMPLATE = lib
TARGET = installer
DEPENDPATH += . \
    .. \
    ../common \
    kdtools/KDToolsCore \
    kdtools/KDUpdater
INCLUDEPATH += . \
    .. \
    kdtools

DESTDIR = $$OUT_PWD/../lib
DLLDESTDIR = $$OUT_PWD/../bin

DEFINES += QT_NO_CAST_FROM_ASCII \
    BUILD_LIB_INSTALLER \
    FSENGINE_TCP

CONFIG( shared, static|shared ){
    DEFINES += KDTOOLS_SHARED
}

QT += script \
    network \
    sql
CONFIG += help uitools

QTPLUGIN += qsqlite

include(3rdparty/p7zip_9.04/p7zip.pri)
include(kdtools/KDToolsCore/KDToolsCore.pri)
include(kdtools/KDUpdater/KDUpdater.pri)

HEADERS += $$PWD/qinstaller.h \
    $$PWD/qinstallergui.h \
    ../common/binaryformat.h \
    ../common/binaryformatengine.h \
    ../common/binaryformatenginehandler.h \
    ../common/repository.h \
    ../common/zipjob.h \
    ../common/kd7zengine.h \
    ../common/kd7zenginehandler.h \
    ../common/utils.h \
    ../common/errors.h \
    kdmmappedfileiodevice.h \
    qinstallercomponent.h \
    qinstallercomponentmodel.h \
    qinstallerglobal.h \
    qtpatch.h \
    persistentsettings.h \
    projectexplorer_export.h \
    qtpatchoperation.h \
    setdemospathonqtoperation.h \
    setexamplespathonqtoperation.h \
    setpluginpathonqtcoreoperation.h \
    setimportspathonqtcoreoperation.h \
    replaceoperation.h \
    linereplaceoperation.h \
    registerdocumentationoperation.h \
    registerqtoperation.h  \
    registerqtv2operation.h  \
    registertoolchainoperation.h  \
    setqtcreatorvalueoperation.h \
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
    installersettings.h \
    getrepositorymetainfojob.h \
    downloadarchivesjob.h \
    init.h \
    updater.h \
    updateagent.h \
    updatesettings.h \
    updatesettingsdialog.h \
    updatesettingswidget.h \
    componentselectiondialog.h \
    adminauthorization.h \
    fsengineclient.h \
    fsengineserver.h \
    elevatedexecuteoperation.h \
    installationprogressdialog.h \
    fakestopprocessforupdateoperation.h \
    lazyplaintextedit.h \
    progresscoordinator.h \
    minimumprogressoperation.h \
    performinstallationform.h \
    messageboxhandler.h \
    getrepositoriesmetainfojob.h \
    licenseoperation.h \
    qtcreator_constants.h \
    qtcreatorpersistentsettings.h

SOURCES += $$PWD/qinstaller.cpp \
    $$PWD/qinstallergui.cpp \
    ../common/binaryformat.cpp \
    ../common/binaryformatengine.cpp \
    ../common/binaryformatenginehandler.cpp \
    ../common/repository.cpp \
    ../common/zipjob.cpp \
    ../common/kd7zengine.cpp \
    ../common/kd7zenginehandler.cpp \
    ../common/installersettings.cpp \
    ../common/fileutils.cpp \
    ../common/utils.cpp \
    kdmmappedfileiodevice.cpp \
    qinstallercomponent.cpp \
    qinstallercomponentmodel.cpp \
    qtpatch.cpp \
    persistentsettings.cpp \
    qtpatchoperation.cpp  \
    setdemospathonqtoperation.cpp \
    setexamplespathonqtoperation.cpp \
    setpluginpathonqtcoreoperation.cpp \
    setimportspathonqtcoreoperation.cpp \
    replaceoperation.cpp \
    linereplaceoperation.cpp \
    registerdocumentationoperation.cpp \
    registerqtoperation.cpp \
    registerqtv2operation.cpp \
    registertoolchainoperation.cpp  \
    setqtcreatorvalueoperation.cpp \
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
    updateagent.cpp \
    updatesettings.cpp \
    updatesettingsdialog.cpp \
    updatesettingswidget.cpp \
    componentselectiondialog.cpp \
    adminauthorization.cpp \
    fsengineclient.cpp \
    fsengineserver.cpp \
    elevatedexecuteoperation.cpp \
    installationprogressdialog.cpp \
    fakestopprocessforupdateoperation.cpp \
    lazyplaintextedit.cpp \
    progresscoordinator.cpp \
    minimumprogressoperation.cpp \
    performinstallationform.cpp \
    messageboxhandler.cpp \
    getrepositoriesmetainfojob.cpp \
    licenseoperation.cpp \
    qtcreatorpersistentsettings.cpp


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

# Needed by KDToolsCore (in kdlog_win.cpp):
win32:LIBS += advapi32.lib psapi.lib
macx:LIBS += -framework Carbon

CONFIG( shared, static|shared ): {
  DEFINES += LIB_INSTALLER_SHARED
  win32: LIBS += shell32.lib
}

macx: LIBS += -framework Security
    
TRANSLATIONS += de_de.ts \
    sv_se.ts
RESOURCES += ../common/openssl.qrc \
             patch_file_lists.qrc

FORMS += componentselectiondialog.ui updatesettingsdialog.ui updatesettingswidget.ui
