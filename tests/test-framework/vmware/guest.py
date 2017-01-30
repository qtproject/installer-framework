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

# -*- coding: utf-8 -*-
import sys, subprocess, ConfigParser, platform, os

# begin Autobuild/helpers/runcommand.py

import os
import re
import time
import sys
import signal
from subprocess import Popen
from threading import Thread
from subprocess import PIPE
import subprocess
import platform

def DebugN( l, m ):
    pass

def windowskill( pid ):
	""" replace os.kill on Windows, where it is not available"""
	Cmd = 'taskkill /PID ' + str( int( pid ) ) + ' /T /F' 
	if os.system( Cmd ) == 0:
		DebugN( 4, 'windowskill: process ' + str( pid ) + ' killed.' )
		
def kill( pid, signal ):
	if 'Windows' in platform.platform():
		windowskill( pid )
	else:
		os.kill( pid, signal )
	
class CommandRunner( Thread ):
	def __init__ ( self, Cmd ):
		Thread.__init__( self )
		self.__started = None
		self.mCmd = Cmd
		self.mOutput = ()
		self.mPid = -1
		self.mReturnCode = -1
		self.mError = ()
		self.__combinedOutput = False
		
	def setCombinedOutput(self, combine):
		if combine: # make sure combine is usable as a boolean
			self.__combinedOutput = True
		else:
			self.__combinedOutput = False

	def getCombineOutput(self):
		return self.__combinedOutput

	def run( self ):
		DebugN( 4, 'RunCommand: ' +  str( self.mCmd ) )
		stderrValue = subprocess.PIPE
		if self.__combinedOutput:
			stderrValue = subprocess.STDOUT
		self.__started = True
#nokia-sdk: changed to shell=False, as shell=True requires manual quoting of args
		p = Popen ( self.mCmd, shell = False, stdout=subprocess.PIPE, stderr=stderrValue )
		self.mPid = p.pid
		self.mOutput, self.mError = p.communicate()
		self.mReturnCode = p.returncode
		DebugN( 4, 'ReturnCode of RunCommand: ' + str( p.returncode ) )

	def started(self):
		return self.__started

	def output( self ):
		return self.mOutput, self.mError

	def terminate( self ):
		# FIXME logic?
		if self.mPid != -1:
			kill( self.mPid, signal.SIGTERM )
		if self.mPid == True:
			self.join( 5 )
			kill( self.mPid, signal.SIGKILL )
		self.mPid = -1


def RunCommand( Cmd, TimeOutSeconds=-1, CombineOutput = False ):
	timeoutString = ' without a timeout'
	if TimeOutSeconds > 0:
		timeoutString = ' with timeout of ' + str( TimeOutSeconds )
	combinedOutputString = ' and separate output for stdout and stderr'
	if CombineOutput:
		combinedOutputString = ' and combined stdout and stderr output'
	DebugN ( 3, 'RunCommand: executing ' + str( Cmd ) + timeoutString + combinedOutputString )
	runner = CommandRunner ( Cmd )
	runner.setCombinedOutput( CombineOutput )
	runner.start()
	# poor man's mutex:
	while not runner.started():
		time.sleep( 0.1 )
#	if "CYGWIN" in platform.platform() or 'Windows' in platform.platform():
#		time.sleep( 1 )
	if TimeOutSeconds == -1:
		runner.join()
	else:
		runner.join( TimeOutSeconds )
	if runner.isAlive():
		runner.terminate()
#		if "CYGWIN" in platform.platform() or 'Windows' in platform.platform():
#			time.sleep(1)
		runner.join( 5 )
		DebugN( 3, 'RunCommand: command timed out, returncode is ' + str( runner.mReturnCode ) )
		return ( runner.mReturnCode, runner.output(), True )
	else:
		DebugN( 3, 'RunCommand: command completed, returncode is ' + str( runner.mReturnCode ) )
		return ( runner.mReturnCode, runner.output(), False )

# end Autobuild/helpers/runcommand.py


def printUsage():
    print( "Usage: {0} <outputfile> <timeout> <program> [<args>*]".format( sys.argv[0] ) )
    
if len( sys.argv ) < 4:
    printUsage()
    sys.exit( 1 )

output = sys.argv[1]
timeout = int( sys.argv[2] )
cmd = sys.argv[3:]

start = time.clock()
exitCode, pout, timedOut = RunCommand( cmd, timeout )
end = time.clock()

if timedOut:
    exitStatus = 'Timeout'
else:
    exitStatus = 'Normal' #TODO: detect crash

config = ConfigParser.SafeConfigParser()
config.add_section( 'Result' )
config.set( 'Result', 'ExitCode', str( exitCode ) )
config.set( 'Result', 'ExitStatus', exitStatus )
config.set( 'Result', 'Filename', cmd[0] )
config.set( 'Result', 'Arguments', " ".join( cmd[1:] ) )
config.set( 'Result', 'Timeout', str( timeout ) )
config.set( 'Result', 'ExecutionTime', str( end - start ) )
config.set( 'Result', 'Stdout', pout[0] )
config.set( 'Result', 'Stderr', pout[1] )

config.add_section( 'Platform' )
config.set( 'Platform', 'system', platform.system() )
config.set( 'Platform', 'release', platform.release() )
config.set( 'Platform', 'version', platform.version() )
config.set( 'Platform', 'machine', platform.machine() )

with open( output, 'w' ) as configFile:
    config.write(configFile )

sys.exit( exitCode )




