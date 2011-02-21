# -*- coding: utf-8 -*-
import files, os, string, platform
from testexception import TestException

if ( platform.system() == "Windows" ):
    import registry

def makeAbsolutePath( path, relativeTo ):
    if os.path.isabs( path ) or relativeTo == None:
        return path
    else:
        return relativeTo + os.sep + path
        

class TestRunner:
    def __init__( self, testDir, basedir, result ):
        self._testDir = testDir
        self._basedir = basedir
        self._result = result
        
    def checkFile( self, name, size=-1, expectedMd5=None ):
        try:
            files.checkFileImpl( name, size, expectedMd5 )
        except TestException as e:
            self._result.addFailed( name, e.value )
    
    def checkFileList( self, path ):
        lineNum = 0
        haveError = False
        with open( path, 'r' ) as f:
            while True:
                line = f.readline()
                lineNum += 1
                if not line:
                    break
                line = string.strip( line )
                if len( line ) == 0:
                    continue
                segments = string.split( line, ';' )
                if len( segments ) == 3:
                    fp = makeAbsolutePath( segments[0], self._basedir )
                    try:
                        fs = int( segments[1] )
                    except ValueError:
                        fs = -1 #TODO handle error
                    femd5 = segments[2]
                    femd5 = string.strip( femd5 )
                    self.checkFile( fp, fs, femd5 )
                else:
                    self._result.addFailed( path + '_' + str( lineNum ), "Could not parse file list entry: " + line )
                    haveError = True
        if not haveError:
            self._result.addPassed( path, "" )
 
    def checkRegistryList( self, path ):
        haveError = False
        lineNum = 0
        with open( path, 'r' ) as f:
            while True:
                lineNum += 1
                line = f.readline()
                if not line:
                    break
                line = string.strip( line )
                if len( line ) == 0:
                    continue
                segments = string.split( line, ';' )
                if len( segments ) == 3:
                    key = segments[0].strip()
                    value = segments[1].strip()
                    expectedData = segments[2].strip()
                    registry.checkKey( key, value, expectedData )
                else:
                    self._result.addFailed( path + '_' + str( lineNum ), "Could not parse registry list entry: " + line )
                    haveError = True
        if not haveError:
              self._result.addPassed( path, "" )

    def run( self ):
        fileLists = files.locateFiles( self._testDir, "*.filelist" )
        for i in fileLists:
            self.checkFileList( i )
            
        if ( platform.system() == "Windows" ):
            registryLists = files.locateFiles( self._testDir, "*.registrylist" )
            for i in registryLists:
                self.checkRegistryList( i )
        
        # run all .py's in testdir
        # execute all filelists
