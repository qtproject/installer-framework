import ConfigParser, os, utils

class Step:
    def __init__( self, installscript, checkerTestDir, timeout ):
        self._installscript = installscript
        self._checkerTestDir = checkerTestDir
        self._timeout = timeout
    
    def installscript( self ):
        return self._installscript
    
    def checkerTestDir( self ):
        return self._checkerTestDir
    
    def timeout( self ):
        return self._timeout
    
class TestCase:
    def __init__( self, path ):
        self._steps = []
        config = ConfigParser.SafeConfigParser()
        config.read( path )
        self._path = path
        self._platforms = []
        found = True
        stepNum = 0
        while found:
            sec = "Step{0}".format( stepNum )
            if not config.has_section( sec ):
                found = False
                continue
            
            stepNum += 1
            installscript = utils.makeAbsolutePath( utils.get_config_option( config, None, "installscript", None, sec ), os.path.dirname( path ) )
            checkerTestDir = utils.get_config_option( config, None, "checkerTestDir", None, sec )
            checkerTestDir = utils.makeAbsolutePath( checkerTestDir, os.path.dirname( path ) ) if checkerTestDir else ""
            timeout = int( utils.get_config_option( config, None, "timeout", 60 * 60, sec ) )
            self._steps.append( Step( installscript, checkerTestDir, timeout ) )
             
        self._name = utils.get_config_option( config, None, "name", utils.basename( path ) )
        self._targetDirectory = utils.get_config_option( config, None, "targetDirectory", "" )
        self._maintenanceToolLocation = utils.get_config_option( config, None, "maintenanceToolLocation", "" )
        platforms = utils.get_config_option( config, None, "platforms" )
        if platforms != None:
            self._platforms = platforms.split( ',' )

    def supportsPlatform( self, platform ):
        return platform in self._platforms

    def installerTimeout( self ):
        return self._installerTimeout
    
    def platforms( self ):
        return self._platforms
    
    def name( self ):
        return self._name
    
    def targetDirectory( self ):
        return self._targetDirectory
    
    def steps( self ):
        return self._steps 

    def path( self ):
        return self._path

    def maintenanceToolLocation( self ):
        return self._maintenanceToolLocation
    