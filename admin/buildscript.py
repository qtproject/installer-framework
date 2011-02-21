#!/usr/bin/python
# -*- coding: utf-8 -*-
import copy, platform, os, re
from AutobuildCore.Configuration import Configuration
from AutobuildCore.Project import Project
from AutobuildCore.helpers.build_script_helpers import DebugN
from AutobuildCore.helpers.build_script_helpers import AddToPathCollection
from AutobuildCore.helpers.platformdefs import PlatformDefs
from AutobuildCore.helpers.exceptdefs import AutobuildException
from AutobuildCore import autobuildRoot
from AutobuildCore import Callback

hiddenFileIdent = '.'

buildSequenceSwitches = ''

#if 'Windows' in platform.platform():
#	buildSequenceSwitches += ',disable-conf-bin-package'
#	hiddenFileIdent = '_'


class KDToolsBuilder( Callback.ConfigurationCallback ):
	def __init__( self, platform ):
		Callback.ConfigurationCallback.__init__( self, 'KD Tools Builder' )
		self.__platform = platform

	def preBuildCallback( self, job ):
		config = job.configuration()
		project = config.project()
		kdtoolsFolder = job.buildDir() + os.sep + 'kdtools' + os.sep + 'kdtools'
		if 'Windows' in self.__platform:
			confcmd = 'configure.bat ' + config.getOptions()
		else:
			confcmd = './configure.sh ' + config.getOptions()
		step = job.executomat().step( 'conf-configure' )
		step.addPreCommand( confcmd, kdtoolsFolder )
		defs = PlatformDefs.GivePlatformDefs()
		makecmd = defs.makeProgram()
		step.addPreCommand( makecmd, kdtoolsFolder )
		maketestcmd = defs.makeProgram() + ' test'
		step.addPreCommand( maketestcmd, kdtoolsFolder )
		AddToPathCollection( defs.libPathVariable() , kdtoolsFolder + os.sep + 'lib' )

scmPath = 'svn+ssh://svn.kdab.net/home/SVN-klaralv/projects/Nokia/SDK'
product = Project( 'Installer' )

product.setScmUrl( scmPath + '/trunk' )
#product.setPackageLocation( 'svn.kdab.net:/home/build/autobuild/packages/kdchart' )
product.setBuildSequenceSwitches( 's', buildSequenceSwitches )
product.setBuildSequenceSwitches( 'f', buildSequenceSwitches )
product.getSettings().addRecipientOnSuccess( 'nokia-sdk@kdab.net' )
product.getSettings().addRecipientOnFailure( 'nokia-sdk@kdab.net' )

debug = Configuration( product, 'Debug' )
debug.addCallback( KDToolsBuilder( platform.platform() ) )
debug.setBuilder('autotools')
debug.setPackageDependencies( [ 'Qt-4.[5-9].?-Static-Debug' ] )
debug.setBuildMode( 'inSource' )
debug.setOptions( '-static -debug' )
debug.setBuildTypes('MCDFE')

release = copy.copy( debug ) # use debug as the base configuration
release.setConfigName( 'Release' )
release.setPackageDependencies( [ 'Qt-4.[5-9].?-Static-Release' ] )
release.setOptions( '-static -release' )
release.setBuildTypes( 'MCDSFE' ) # snapshots are release builds


jobs = [ debug, release ]
#jobs = [ debug, eval ]

product.build( jobs )
