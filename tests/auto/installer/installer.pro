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
    metadatajob

win32 {
    SUBDIRS += registerfiletypeoperation
}

linux-g++* {
    SUBDIRS += createdesktopentryoperation
}

scriptengine.depends += unicodeexecutable
