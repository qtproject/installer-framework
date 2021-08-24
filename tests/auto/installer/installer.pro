TEMPLATE = subdirs

SUBDIRS += \
    archivefactory \
    settings \
    repository \
    compareversion\
    componentidentifier \
    componentmodel \
    fakestopprocessforupdateoperation \
    messageboxhandler \
    extractarchiveoperationtest \
    lib7zarchive \
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
    environmentvariableoperation \
    licenseagreement \
    globalsettingsoperation \
    elevatedexecuteoperation \
    treename \
    createoffline \
    contentshaupdate

CONFIG(libarchive) {
    SUBDIRS += libarchivearchive
}

win32 {
    SUBDIRS += registerfiletypeoperation \
        createshortcutoperation
}

linux-g++* {
    SUBDIRS += \
        createdesktopentryoperation \
        installiconsoperation
}

scriptengine.depends += unicodeexecutable
