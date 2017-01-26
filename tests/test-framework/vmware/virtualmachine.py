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


import commands, ConfigParser, os, string, utils, platform

class VMException(Exception):
  def __init__(self, value):
      self.value = value
  def __str__(self):
      return repr(self.value)

class VirtualMachine:
    def __init__( self, vmrun, vmx, username, password, tempDir, ostype ):
        self._vmrun = vmrun
        self._vmx = vmx
        self._username = username
        self._password = password
        self._tempDir = tempDir
        self._ostype = ostype
        self._useGui = True
        self._name = "no name"
        self._hostType = ""
        self._hostLocation = None
        self._hostUsername = None
        self._hostPassword = None
        
    def setGuiEnabled( self, usegui ):
        self._useGui = usegui
    
    def name( self ):
        return self._name

    def isRemote( self ):
        return len( self._hostType ) > 0;

    def setRemoteHost( self, type, loc, user, pw ):
        self._hostType = type
        self._hostLocation = loc
        self._hostUsername = user
        self._hostPassword = pw

 
    def command( self, message, command, parameters=None ):
        if len( self._hostType ) > 0:
            remote = "-T {0} -h '{1}' -u {2} -p {3}".format( self._hostType, self._hostLocation, self._hostUsername, self._hostPassword )
        else:
            remote = ""
        vmcommand = "'{0}' {1} -gu {2} -gp {3} {4} '{5}' ".format( self._vmrun, remote, self._username, self._password, command, self._vmx )
        if parameters:
            vmcommand += parameters
        print "** " + message
        status, output = commands.getstatusoutput(vmcommand)
        if status != 0:
            print "* Return code: {0}".format(status)
            print "* Output:"
            print output
            print
        return status, output

    def copyFileToGuest( self, source, target ):
        #TODO convert paths to guest system?
        status, _ = self.command( 'Copying {0} to {1}'.format( source, target ), 'copyFileFromHostToGuest', "'{0}' '{1}'".format( source, target ) )
        if status != 0:
            raise VMException( "Could not copy from host to guest: source {0} target {1}".format( source, target ) )

    def copyFileFromGuest( self, source, target ):
        #TODO convert paths to guest system?
        status, _ = self.command( 'Copying {0} to {1}'.format( source, target ), 'copyFileFromGuestToHost', "'{0}' '{1}'".format( source, target ) )
        if status != 0:
            raise VMException( "Could not copy from guest to host: source {0} target {1}".format( source, target ) )


    def ostype( self ):
        return self._ostype
    
    def vmxPath( self ):
        return self._vmx
    
    def mkTempPath( self, filename ):
        return self._tempDir + self.pathSep() + utils.basename( filename )
        
    def pathSep( self ):
        if self._ostype == "windows":
            return "\\"
        else:
            return "/"
        
    def copyToTemp( self, source, targetN=None ):
        if source == None or len( string.strip( source ) ) == 0:
            return None
        if targetN == None:
            targetN = source

        target = self.mkTempPath( targetN )
        self.copyFileToGuest( source, target )
        return target

    def copyFromTemp( self, filename, target ):
        source = self.mkTempPath( filename )
        self.copyFileFromGuest( source, target )
        return target
    
    def start( self ):
        arg = "gui" if self._useGui else "nogui"
        self.command("Starting VM ({0})".format( arg ), "start", arg )
        
    def kill( self ):
        self.command("Stopping VM", "stop", "hard" )
  
    def isRunning( self ):
        _, vmList = self.command("Checking running VMs", "list")
        _, _, vmFilename = self._vmx.rpartition( os.sep )
        return vmList.find(vmFilename) is not -1

    def snapshotExists( self, snapshot ):
        _, output = self.command("Checking snapshots", "listSnapshots")
        snapshotList = output.split("\n")
        # Remove first entry which contains number of snapshots
        snapshotList.pop(0)
        return snapshotList.count( snapshot ) > 0

    def checkPythonInstalled( self ):
        pp = utils.unixPathSep( self._python )
        pythonStatus, _ = self.command("Checking for guest installed Python", "fileExistsInGuest", pp )
        print pythonStatus
        if pythonStatus != 0:
            raise VMException("Could not find python in {0}: Please specify the path/install Python in the VM from http://www.python.org/download/".format( pp ) )
        else:
            print("Python found ({0})".format( pp ) )

    def python( self ):
        return self._python
    
    def snapshot( self ):
        return self._snapshot
    
    def revertToSnapshot( self, snapshot=None ):
        if snapshot != None:
            snap = snapshot
        else:
            snap = self._snapshot
        # VMware Fusion needs to be closed before you can restore snapshots so kill it
        if ( platform.system() == "Darwin" ):
            commands.getstatusoutput( "ps x|grep 'VMware Fusion'|cut -d ' ' -f1|xargs kill" )
        return self.command("Reverting to '{0}' snapshot".format( snap ), "revertToSnapshot", snap )

    def createSnapshot( self, name ): 
        return self.command("Creating error snapshot '{0}'".format( name ), "snapshot", name )


def fromVMRunAndPath( vmrun, path ):
    config = ConfigParser.SafeConfigParser()
    config.read( path )

    hostType = utils.get_config_option( config, None, "type", "", "Host" )
    hostLocation = utils.get_config_option( config, None, "location", "", "Host" )
    hostUsername = utils.get_config_option( config, None, "username", "", "Host" )
    hostPassword = utils.get_config_option( config, None, "password", "", "Host" )

    vmxVal = utils.get_config_option( config, None, "vmx" )
    if not len( hostType ) == 0:
        vmx = utils.makeAbsolutePath( vmxVal, os.path.dirname( path ) )
    else:
        vmx = vmxVal

    username = utils.get_config_option( config, None, "username", "nokia" )
    password = utils.get_config_option( config, None, "password", "nokia" )
    tempDir = utils.get_config_option( config, None, "tempDir", "c:\\windows\\temp" )
    ostype = utils.get_config_option( config, None, "os", "windows" )
    
    vm = VirtualMachine( vmrun, vmx, username, password, tempDir, ostype )
    vm._name = utils.get_config_option( config, None, "name", utils.basename( path ) )
    vm._snapshot = utils.get_config_option( config, None, "snapshot", "base" )
    vm._python = utils.get_config_option( config, None, "python", "c:/python26/python.exe" )
    vm._hostType = hostType
    vm._hostLocation = hostLocation
    vm._hostUsername = hostUsername
    vm._hostPassword = hostPassword
    return vm
