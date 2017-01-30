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


import ConfigParser, datetime, os, string, sys, time, platform
import testcase, utils, result, virtualmachine
from virtualmachine import VMException
from xml.sax import make_parser
from xml.sax.handler import ContentHandler

class ControlException( Exception ):
  def __init__( self, value ):
      self.value = value
  def __str__( self ):
      return repr( self.value )

class Handler( ContentHandler ):
    def __init__( self, res ):
        self._res = res
        self._inResult = False
        self._buf = ""
        
    def startElement( self, name, attrs ):
        self._inResult = False
        if name == "result":
            self._inResult = True   
            self._name = attrs['name']
            self._status = attrs['status']

    def endElement( self, name ):
        if name == 'result':
            trimmed = string.strip( self._buf )
            if self._status == "passed":
                stat = result.CheckerResult.Passed
            else:
                stat = result.CheckerResult.Failed
            self._res.addCheckerResult( result.CheckerResult( self._name, stat, trimmed ) )
            self._inResult = False

        self._buf = ""
        
    def characters( self, ch ):
        if self._inResult:
            self._buf += ch
        
class Control:
    def __init__( self, vmrun, checkerDir, source, reporter ):
        self._vmrun = vmrun
        self._checkerDir = checkerDir
        self._vms = []
        self._testcases = []
        self._source = source
        self._reporter = reporter
        self._guiEnabled = True
        self._createErrorSnapshots = False
        self._hostType = ""
        self._hostLocation = ""
        self._hostUsername = ""
        self._hostPassword = ""
        
    def setGuiEnabled( self, usegui ):
        self._guiEnabled = usegui
        for i in self._vms:
            i.setGuiEnabled( usegui )

    def setCreateErrorSnapshots( self, createSnapshots ):
        self._createErrorSnapshots = createSnapshots

    def setRemoteHost( self, type, loc, user, pw ):
        self._hostType = type
        self._hostLocation = loc
        self._hostUsername = user
        self._hostPassword = pw
        for vm in self._vms:
            if not vm.isRemote():
                vm.setRemoteHost( self._hostType, self._hostLocation, self._hostUsername, self._hostPassword )

    def addVM( self, cfgpath ):
        config = ConfigParser.SafeConfigParser()
        config.read( cfgpath )
        vm = virtualmachine.fromVMRunAndPath( self._vmrun, cfgpath )
        vm.setGuiEnabled( self._guiEnabled )
        if len( self._hostType ) > 0 and not vm.isRemote():
            vm.setRemoteHost( self._hostType, self._hostLocation, self._hostUsername, self._hostPassword )
        self._vms.append( vm )
        #TODO catch/transform exceptions
        
    def addTestCase( self, path ):
        self._testcases.append( testcase.TestCase( path ) )
        

    def run( self ):
        while True:
            try:
                inst = self._source.nextInstaller()
                if inst == None:
                    print( "** Installer source returned None, aborting" )
                    return
                if inst.error:
                    raise ControlException( inst.error )
                print( "** New installer: {0}".format( inst.path ) )
                self.testInstaller( inst, inst.platform )
            except KeyboardInterrupt:
                raise
            except:
                self._reporter.reportException()
                
    def testInstaller( self, inst, platform ):
        for vm in self._vms:
            if vm.ostype() != platform:
                continue
            for case in self._testcases:
                if not case.supportsPlatform( platform ):
                    continue
                res = result.Result()
                try:
                    try:
                        res.setInstaller( inst )
                        res.setTestCase( case )
                        res.setVirtualMachine( vm )
                        res.testStarted()
                        self.run_test( inst.path, vm, case, res )
                        res.testFinished()
                        inst.markAsTested()
                    except KeyboardInterrupt:
                        raise
                    except:
                        res.addException()
                finally:
                    self._reporter.reportResult( res )
                    
    def convertCheckerResults( self, filename, res ):
        parser = make_parser()
        parser.setContentHandler( Handler( res ) )
        f = file( filename, 'rb' )
        parser.parse( f )
    
    def run_test( self, installerPath, vm, testcase, res ):  
        steps = testcase.steps()
        if len( steps ) == 0:
            raise ControlException( "No steps found for testcase {0}".format( testcase.name() ) )

        revertStatus, _ = vm.revertToSnapshot()
        if revertStatus != 0:
            raise VMException( "Failed to revert to snapshot '{0}'".format( vm.snapshot() ) )

        
        time.sleep( 5 ) # Trying to avoid a possible race between restore and start
        
        vm.start()

        try:
            try:
                vm.checkPythonInstalled()        
                wrapperpath = vm.copyToTemp( utils.execution_path( 'guest.py' ) )
                
                for stepNum in range( len( steps ) ):
                    needSnapshot = False
                    step = steps[stepNum]
                    if stepNum == 0:               
                        executableguestpath = vm.copyToTemp( installerPath )
                    else:
                        executableguestpath = testcase.maintenanceToolLocation()
    
                    outputFileName = 'output{0}.log'.format( stepNum )
                    outputpath = vm.mkTempPath( outputFileName  )
                    scriptguestpath = vm.copyToTemp( step.installscript() )
                    timeout = step.timeout()
                    checkerguestpath = vm.copyToTemp( step.checkerTestDir(), "checkerTestDir{0}".format( stepNum ) ) if len( string.strip( step.checkerTestDir() ) ) > 0 else None
                    vm.command( 'Execute installer', "runProgramInGuest", "'{0}' '{1}' '{2}' '{3}' '{4}' --script '{5}'".format( vm.python(), wrapperpath, outputpath, timeout, executableguestpath, scriptguestpath ) )            
                    vm.copyFromTemp( outputFileName, outputFileName )
                    r = ConfigParser.SafeConfigParser()
                    r.read( outputFileName )
                    try:
                        s = r.get( 'Result', 'ExitCode' )
                        exitCode = int( s )
                    except ValueError:
                        res.addInternalError( "Could not parse integer exit code from '{0}'".format( r.get( 'Result', 'ExitCode' ) ) )                
                        exitCode = -1
                    try:
                        s = r.get( 'Result', 'ExecutionTime' )
                        executionTime = float( s )
                    except ValueError:
                        res.addInternalError( "Could not parse float execution time from '{0}'".format( r.get( 'Result', 'ExecutionTime' ) ) )                
                        executionTime = 0.0 
        
                    exitStatus = result.exitStatusFromString( r.get( 'Result', 'ExitStatus' ) )
                    instR = result.ExecutionResult( exitCode, exitStatus, executionTime )
                    
                    if instR.hasError():
                        needSnapshot = True
                    
                    checkerResults = []
                    if checkerguestpath and not instR.hasError():
                        if ( platform.system() == "Darwin" ):
                            # Have to sleep to work around VMware Fusion bug
                            time.sleep( 30 )
                        run_py = vm.copyToTemp( self._checkerDir ) + vm.pathSep() + "run.py"
                        if ( platform.system() == "Darwin" ):
                            # Have to sleep to work around VMware Fusion bug
                            time.sleep( 30 )
                        checkeroutputFileName = 'checker-output{0}.xml'.format( stepNum )
                        checkeroutput = vm.mkTempPath( checkeroutputFileName )
                        vm.command( 'Execute checker tests', "runProgramInGuest", "'{0}' '{1}' '{2}' -o '{3}' -p '{4}'".format( vm.python(), run_py, checkerguestpath, checkeroutput, testcase.targetDirectory() ) )            
                        vm.copyFromTemp( checkeroutputFileName, checkeroutputFileName )
                        self.convertCheckerResults( localcheckeroutput, checkerResults )
                        if res.hasCheckerErrors():
                            needSnapshot = True
                    if self._createErrorSnapshots and needSnapshot:
                        snapshot = 'error-{0}-{1}'.format( datetime.datetime.now().strftime( '%Y%m%d_%H%M%S' ), utils.randomString( 4 ) )
                        status, _ = vm.createSnapshot( snapshot )
                        if status == 0:
                            res.setErrorSnapshot( snapshot )
                        else:
                            res.addInternalError( 'Could not create error snapshot "{0}"'.format( snapshot ) )
                    res.addStepResult( result.StepResult( instR, checkerResults ) )
                #TODO handle timeouts?
            finally:
                vm.kill()
        except e:
            print( e )
            res.addInternalError( str( e ) )
     