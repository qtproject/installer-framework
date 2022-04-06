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
    contentshaupdate \
    componentreplace

CONFIG(libarchive) {
    SUBDIRS += libarchivearchive
}

CONFIG(lzmasdk) {
    SUBDIRS += lib7zarchive
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
