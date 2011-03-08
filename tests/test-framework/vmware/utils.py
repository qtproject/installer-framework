# -*- coding: utf-8 -*-
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
