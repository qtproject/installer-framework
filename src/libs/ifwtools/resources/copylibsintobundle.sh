#!/bin/sh
#############################################################################
##
## Copyright (C) 2017 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the Qt Installer Framework.
##
## $QT_BEGIN_LICENSE:GPL-EXCEPT$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 3 as published by the Free Software
## Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################


# this script puts all libs directly needed by the bundle into it

QTDIR=""
IS_DEBUG=0
HAVE_CORE=0
HAVE_SVG=0
HAVE_PHONON=0
HAVE_SCRIPT=0
HAVE_SQL=0
HAVE_WEBKIT=0

function handleFile()
{
    local FILE=$1
    local BUNDLE=$2

    # all dynamic libs directly needed by the bundle, which are not in /System/Library or in /usr/lib (which are system default libs, which we don't want)
    local LIBS=`xcrun otool -L $FILE | grep -v 'executable_path' | grep -v '/System/Library' | grep -v '/usr/lib' | grep '/' | sed -ne 's,^ *\(.*\) (.*,\1,p'`

    local lib
    for lib in $LIBS; do
        local NAME=`basename $lib`
        
        if echo $NAME | grep 'QtCore' >/dev/null; then
            HAVE_CORE=1
            QTDIR=`echo $lib | sed -ne 's,^\(.*\)/lib/[^/]*QtCore.*$,\1,p'`
            if echo $NAME | grep 'debug' >/dev/null; then
                IS_DEBUG=1
            fi
        elif echo $NAME | grep 'QtSvg' >/dev/null; then
            HAVE_SVG=1
        elif echo $NAME | grep 'phonon' >/dev/null; then
            HAVE_PHONON=1
        elif echo $NAME | grep 'QtScript' >/dev/null; then
            HAVE_SCRIPT=1
        elif echo $NAME | grep 'QtSql' >/dev/null; then
            HAVE_SQL=1
        elif echo $NAME | grep 'QtWebKit' >/dev/null; then
            HAVE_WEBKIT=1
        fi

        if [ `basename $FILE` != $NAME ]; then

        # this part handles libraries which are macOS frameworks
        if echo $lib | grep '\.framework' >/dev/null; then
            local FRAMEWORKPATH=`echo $lib | sed -ne 's,\(.*\.framework\).*,\1,p'`
            local FRAMEWORKNAME=`basename $FRAMEWORKPATH`
            local NEWFRAMEWORKPATH=`echo $lib | sed -ne "s,.*\($FRAMEWORKNAME\),\1,p"`
          
            # Qt installed via the precompled binaries...
            if [ $FRAMEWORKPATH = $FRAMEWORKNAME ]; then
                FRAMEWORKPATH="/Library/Frameworks/$FRAMEWORKNAME"
                if [ ! -e "$FRAMEWORKPATH" ]; then
                    echo "Framework $FRAMEWORKNAME not found."
                    exit 1
                fi
            fi

            if [ ! -e "$BUNDLE/Contents/Frameworks/$NEWFRAMEWORKPATH" ]; then
                echo Embedding framework $FRAMEWORKNAME

        
                # copy the framework into the bundle
                cp -R $FRAMEWORKPATH $BUNDLE/Contents/Frameworks
                # remove debug libs we've copied
                find $BUNDLE/Contents/Frameworks/$FRAMEWORKNAME -regex '.*_debug\(\.dSYM\)*' | xargs rm -rf

                handleFile "$BUNDLE/Contents/Frameworks/$NEWFRAMEWORKPATH" "$BUNDLE"
            fi
            # and inform the dynamic linker about this
            xcrun install_name_tool -change $lib @executable_path/../Frameworks/$NEWFRAMEWORKPATH $FILE


        # this part handles 'normal' dynamic libraries (.dylib)
        else
            if [ ! -e "$BUNDLE/Contents/Frameworks/$NAME" ]; then
                echo Embedding library $NAME

                # Qt installed via the precompled binaries...
                if [ $lib = $NAME ]; then
                    lib="/Library/Frameworks/$NAME"
                    if [ ! -e "$lib" ]; then
                        lib="/usr/lib/$NAME"
                    fi
                    if [ ! -e "$lib" ]; then
                        echo "Library $NAME not found."
                        exit 1
                    fi
                fi
    
                # copy the lib into the bundle
                cp $lib $BUNDLE/Contents/Frameworks
                handleFile "$BUNDLE/Contents/Frameworks/$NAME" "$BUNDLE"
            fi

            # and inform the dynamic linker about this
            xcrun install_name_tool -change $lib @executable_path/../Frameworks/$NAME $FILE
        fi

        fi
    done
}

function handleQtPlugins()
{
    local PLUGINPATH=$QTDIR/plugins

    # QTDIR was not found, then we're using /Developer/Applications/Qt
    if [ "$PLUGINPATH" = "/plugins" ]; then
        PLUGINPATH="/Developer/Applications/Qt/plugins"
    fi

    CLASS=$1
    EXECUTABLE=$2
    BUNDLE=$3
    mkdir -p $BUNDLE/Contents/plugins/$CLASS
    echo Add $CLASS plugins
    for plugin in `ls $PLUGINPATH/$CLASS/*`; do
        plugin=`basename $plugin`
        if echo $plugin | grep 'debug' >/dev/null; then
            #if [ $IS_DEBUG -eq 1 ]; then
                cp "$PLUGINPATH/$CLASS/$plugin" $BUNDLE/Contents/plugins/$CLASS
                xcrun install_name_tool -change $plugin @executable_path/../plugins/$CLASS/$plugin $EXECUTABLE
                handleFile $BUNDLE/Contents/plugins/$CLASS/$plugin $BUNDLE
            #fi
        else
            #if [ $IS_DEBUG -eq 0 ]; then
                cp "$PLUGINPATH/$CLASS/$plugin" $BUNDLE/Contents/plugins/$CLASS
                xcrun install_name_tool -change $plugin @executable_path/../plugins/$CLASS/$plugin $EXECUTABLE
                handleFile $BUNDLE/Contents/plugins/$CLASS/$plugin $BUNDLE
            #fi
        fi
   done
}

# the app bundle we're working with
BUNDLE=$1
# the executable inside of the bundle
EXECUTABLE=$BUNDLE/Contents/MacOS/`xargs < $BUNDLE/Contents/Info.plist | sed -ne 's,.*<key>CFBundleExecutable</key> <string>\([^<]*\)</string>.*,\1,p'`

mkdir -p $BUNDLE/Contents/Frameworks

handleFile $EXECUTABLE $BUNDLE

if [ $HAVE_CORE -eq 1 ]; then
    handleQtPlugins "imageformats" "$EXECUTABLE" "$BUNDLE"
fi
if [ $HAVE_SVG -eq 1 ]; then
    handleQtPlugins "iconengines" "$EXECUTABLE" "$BUNDLE"
fi
if [ $HAVE_PHONON -eq 1 ]; then
    handleQtPlugins "phonon_backend" "$EXECUTABLE" "$BUNDLE"
fi
if [ $HAVE_SQL -eq 1 ]; then
    handleQtPlugins "sqldrivers" "$EXECUTABLE" "$BUNDLE"
fi
if [ $HAVE_WEBKIT -eq 1 ]; then
    handleQtPlugins "codecs" "$EXECUTABLE" "$BUNDLE"
fi
