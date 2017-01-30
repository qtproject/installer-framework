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

import control, testcase, datetime, source, tempfile, traceback, utils
from source import Installer

class ExecutionResult:
    #return codes:
    Success = 0
    InstallationFailed = 1
    InstallationCanceled = 2
    
    #exit status:
    Normal = 0
    Crash = 1
    Timeout = 2
    
    def __init__( self, exitCode, exitStatus, executionTime ):
        self.exitCode = exitCode
        self.exitStatus = exitStatus
        self.executionTime = executionTime

    def hasError( self ):
        return self.exitCode != 0 or self.exitStatus != ExecutionResult.Normal

class CheckerResult:
    Passed = 0
    Failed = 1
    NotRun = 2

    def hasError( list ):
            #the following would more efficient with something find_if-like (stopping if test with error is found)
            return len( [x for i in list if i.hasError() ] ) > 0
            
    def __init__( self, name, result, errorString ):
        self.name = utils.randomString( 3 ) + '_' + name 
        self.result = result
        self.errorString = errorString
    
    def hasError( self ):
        return self.result != CheckerResult.Passed

class StepResult:
    def __init__( self, executionResult, checkerResults ):
        self.executionResult = executionResult
        self.checkerResults = checkerResults

    def hasCheckerErrors( self ):
            return CheckerResult.hasError( self.checkerResults )
        
def exitStatusFromString( s ):
    if s == 'Crash':
        return ExecutionResult.Crash
    if s == 'Timeout':
        return ExecutionResult.Timeout
    if s == 'Normal':
        return ExecutionResult.Normal
    raise control.ControlException( "Unknown exit status string: {0}".format( s ) )

#there is probably a cooler way with introspection and stuff:
def exitStatusAsString( status ):
    if status == ExecutionResult.Crash:
        return 'Crash'
    if status == ExecutionResult.Normal:
        return 'Normal'
    if status == ExecutionResult.Timeout:
        return 'Timeout'
    raise control.ControlException( "Unknown exit status: {0}".format( status ) )
    
class Result:

    NoError=0 # Everything ok
    InstallerError=1 #Installer failed, or post-conditions not met
    InternalError=2 #Internal test framework error
    
    def __init__( self ):
        self._internalErrors = []
        self._stepResults = []
        self._installer = None
        self._testStart = None
        self._testEnd = None
        self._testcase = None
        self._vm = None
        self._errorSnapshot = None
        self._revision = "revision-todo"

    def setInstaller( self, installer ):
        self._installer = installer
    
    def testStarted( self ):
        self._testStart = datetime.datetime.now()

    def testFinished( self ):
        self._testEnd = datetime.datetime.now()
        
    def setTestCase( self, testcase ):
        self._testcase = testcase
    
    def setErrorSnapshot( self, name ):
        self._errorSnapshot = name
        
    def setVirtualMachine( self, vm ):
        self._vm = vm
        
    def addInternalError( self, errstr ):
        self._internalErrors.append( errstr )

    def hasInternalErrors( self ):
        return len( self._internalErrors ) > 0
    
    def addException( self ):
        s = 'Unexpected exception: {0}'.format( traceback.format_exc( 15 ) )
        self._internalErrors.append( s )
        
    def hasCheckerErrors( self ):
        for step in self._stepResults:
            if CheckerResult.hasError( step.checkerResults ):
                return True
        return False
            
        
    def addStepResult( self, result ):
        self._stepResults.append( result )
            
    def status( self ):
        if len( self._internalErrors ) > 0:
            return Result.InternalError
        if len( self._stepResults ) == 0:
            return Result.InternalError
        for step in self._stepResults:            
            if step.executionResult.exitCode != ExecutionResult.Success or step.exitStatus != ExecutionResult.Normal:
                return Result.InstallerError
            if step.hasCheckerErrors():
                return Result.InstallerError
        #TODO: check test results
        return Result.NoError
        
    def statusAsNiceString( self ):
        s = self.status()
        if s == Result.InternalError:
            return "Internal framework errors"
        if s == Result.InstallerError:
            return "Installation error"
        return "OK"
    
    def constructTitle( self ):
        smiley = ":-)" if self.status() == Result.NoError else ":-("
        
        return "{0}: {1} on {2} testing {3} - {4} {5}".format( self._revision, utils.basename( self._installerSourceLocation ), self._vm.name(), self._testcase.name(), self.statusAsNiceString(), smiley )

