# -*- coding: utf-8 -*-
#!/usr/bin/env python
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


import ConfigParser, inspect, os, string, sys
from random import Random

def findVMRun():
    searchDirectories = os.environ['PATH'].split(os.pathsep)
    searchDirectories.append("/Library/Application Support/VMware Fusion")
    for directory in searchDirectories:
        possibleFile = os.path.join(directory, 'vmrun')
        if os.path.isfile(possibleFile):
            return possibleFile
    return None

def basename( path ):
    if path.endswith( os.path.sep ):
        return os.path.basename( path[0,-1] )
    else:
        return os.path.basename( path )
    
def makeAbsolutePath( path, relativeTo ):
    if os.path.isabs( path ) or relativeTo == None:
        return path
    else:
        return relativeTo + os.sep + path

def execution_path( filename ):
  return os.path.join( os.path.dirname( inspect.getfile( sys._getframe( 1 ) ) ), filename )

def unixPathSep( s ):
    return s.replace( '\\', '/' )

def get_config_option( config, initial, option, default=None, section="General" ):
    # If there's already a valid option from the commandline,
    # override the value from the configuration file.
    if initial != None:
        print( "** Overridden {0}={1}".format( option, initial ) )
        return initial
    try:
        tmp = config.get( section, option )
        print( "** Read config value {0}={1}".format( option, tmp ) )
        return tmp
    except (ConfigParser.NoOptionError, ConfigParser.NoSectionError):
        print( "** Using default value {0}={1}".format( option, default ) )
        return default

def get_enumerated_config_option( config, option ):
    res = []
    i = 0
    while True:
        key = option + str( i )
        val = get_config_option( config, None, key )
        if val == None:
            print res
            return res
        res.append( val )
        i += 1

def randomString( length ):
    return ''.join( Random().sample( string.letters + string.digits, length ) )
