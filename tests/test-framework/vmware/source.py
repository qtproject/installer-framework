# -*- coding: utf-8 -*-
#!/usr/bin/env python
#############################################################################
##
## Copyright (C) 2015 The Qt Company Ltd.
## Contact: http://www.qt.io/licensing/
##
## This file is part of the Qt Installer Framework.
##
## $QT_BEGIN_LICENSE:LGPL$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see http://qt.io/terms-conditions. For further
## information use the contact form at http://www.qt.io/contact-us.
##
## GNU Lesser General Public License Usage
## Alternatively, this file may be used under the terms of the GNU Lesser
## General Public License version 2.1 or version 3 as published by the Free
## Software Foundation and appearing in the file LICENSE.LGPLv21 and
## LICENSE.LGPLv3 included in the packaging of this file. Please review the
## following information to ensure the GNU Lesser General Public License
## requirements will be met: https://www.gnu.org/licenses/lgpl.html and
## http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
##
## As a special exception, The Qt Company gives you certain additional
## rights. These rights are described in The Qt Company LGPL Exception
## version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
##
##
## $QT_END_LICENSE$
##
#############################################################################

import os, time

class Installer:
    def __init__( self, path, platform, location, timestamp=None, tempfile=None ):
        self.path = path
        self.platform = platform
        self.error = None
        self.timestamp = timestamp
        self.tempfile = tempfile
        self.sourceLocation = location

    def markAsTested( self ):
        self.sourceLocation.markAsTested( self.sourceFilename )

class Source:
    def __init__( self ):
        self._dummies = []
        
    def nextInstaller( self ):
        if len( self._dummies ) == 0:
            return None
        delay, path = self._dummies.pop()
        time.sleep( delay )
        if os.path.exists( path ):
            inst = Installer( path, "linux", None )
            inst.sourceFilename = path
            return inst
        else:
            inst = Installer( None, None )
            #simulating download errors
            inst.error = "Installer '{0}' does not exist".format( path )
            return inst
    
    def addDummy( self, delay, path ):
        self._dummies.insert(  0, ( delay, path ) )
