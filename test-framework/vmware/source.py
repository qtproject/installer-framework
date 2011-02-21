# -*- coding: utf-8 -*-
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
