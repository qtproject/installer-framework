TEMPLATE = subdirs

SUBDIRS += \
    settings \
    repository \
    compareversion\
    componentidentifier \
    componentmodel \
    fakestopprocessforupdateoperation \
    messageboxhandler \
    extractarchiveoperationtest \
    lib7zfacade \
    fileutils \
    unicodeexecutable \
    scriptengine \
    consumeoutputoperationtest \
    mkdiroperationtest \
    copyoperationtest \
    solver \
    binaryformat \
    packagemanagercore \
    settingsoperation \
    task \
    clientserver \
    factory \
    replaceoperation \
    brokeninstaller \
    cliinterface \
    linereplaceoperation \
    metadatajob \
    appendfileoperation \
    simplemovefileoperation \
    deleteoperation \
    copydirectoryoperation \
    commandlineupdate \
    moveoperation \
    environmentvariableoperation

win32 {
    SUBDIRS += registerfiletypeoperation
}

linux-g++* {
    SUBDIRS += \
        createdesktopentryoperation \
        installiconsoperation
}

scriptengine.depends += unicodeexecutable
