CONFIG += ordered
TEMPLATE = subdirs
SUBDIRS += extractbinarydata \
    repocompare \
    repogenfromonlinerepo
win32:SUBDIRS += maddehelper

