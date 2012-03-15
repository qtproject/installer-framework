CONFIG += ordered
TEMPLATE = subdirs

SUBDIRS += \
    archivegen \
    binarycreator \
    extractbinarydata \
    repocompare \
    repogen \
    repogenfromonlinerepo
win32:SUBDIRS += maddehelper