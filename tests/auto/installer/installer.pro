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
    rmdiroperationtest \
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
    commandlineinstall \
    linereplaceoperation \
    metadatajob \
    appendfileoperation \
    prependfileoperation \
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
    componentreplace \
    metadatacache \
    contentsha1check \
    componentalias

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
