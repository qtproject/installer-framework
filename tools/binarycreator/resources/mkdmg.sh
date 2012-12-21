#!/bin/sh
#############################################################################
##
## Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
## Contact: http://www.qt-project.org/legal
##
## This file is part of the Qt Installer Framework.
##
## $QT_BEGIN_LICENSE:LGPL$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and Digia.  For licensing terms and
## conditions see http://qt.digia.com/licensing.  For further information
## use the contact form at http://qt.digia.com/contact-us.
##
## GNU Lesser General Public License Usage
## Alternatively, this file may be used under the terms of the GNU Lesser
## General Public License version 2.1 as published by the Free Software
## Foundation and appearing in the file LICENSE.LGPL included in the
## packaging of this file.  Please review the following information to
## ensure the GNU Lesser General Public License version 2.1 requirements
## will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
##
## In addition, as a special exception, Digia gives you certain additional
## rights.  These rights are described in the Digia Qt LGPL Exception
## version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 3.0 as published by the Free Software
## Foundation and appearing in the file LICENSE.GPL included in the
## packaging of this file.  Please review the following information to
## ensure the GNU General Public License version 3.0 requirements will be
## met: http://www.gnu.org/copyleft/gpl.html.
##
##
## $QT_END_LICENSE$
##
#############################################################################

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
