#!/bin/sh
#
# Creates a disk image (dmg) on Mac OS X from the command line.
# usage:
#    mkdmg <volname> <vers> <srcdir>
#
# Where <volname> is the name to use for the mounted image, <vers> is the version
# number of the volume and <srcdir> is where the contents to put on the dmg are.
#
# The result will be a file called <volname>-<vers>.dmg

if [ $# != 2 ]; then
 echo "usage: mkdmg.sh volname srcdir"
 exit 0
fi

VOL="$1"
FILES="$2"
PATHNAME=`dirname $FILES`

DMG=`mktemp "/tmp/$VOL.XXXXXX.dmg"`

# create temporary disk image and format, ejecting when done
SIZE=`du -sk ${FILES} | sed -n 's,^\([0-9]*\).*,\1,p'`
SIZE=$((${SIZE}/1000+1))
hdiutil create "$DMG" -megabytes ${SIZE} -ov -volname "$VOL" -type UDIF -fs HFS+ >/dev/null
DISK=`hdid "$DMG" | sed -ne 's,^\(.*\) *Apple_H.*,\1,p'`
MOUNT=`hdid "$DMG" | sed -ne 's,^.*Apple_HFS[^/]*\(/.*\)$,\1,p'`

# mount and copy files onto volume
cp -R "$PATHNAME/`basename $FILES`" "$MOUNT"
hdiutil eject $DISK >/dev/null

# convert to compressed image, delete temp image
rm -f "$PATHNAME/${VOL}.dmg"
hdiutil convert "$DMG" -format UDZO -o "$PATHNAME/${VOL}.dmg" >/dev/null
rm -f "$DMG"
