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
import ConfigParser, datetime, optparse, os, sys
from functools import partial
import cdashreporter, control, ftpsource, source, reporter, utils

def die( msg ):
    sys.stderr( msg + '\n' )
    sys.exit( 1 )
    
optionParser = optparse.OptionParser(usage="%prog [options] configfile installer0 [installer1 ...]", version="%prog 0.1")
optionParser.add_option("-r", "--vmrun", dest="vmrun", help="vmrun executable to use", metavar="VMRUN" )
optionParser.add_option("-s", "--only-since", dest="since", help="test only installers newer than timestamp (YYYY-MM-DD or YYYY-MM-DD-hh-mm)", metavar="SINCE" )
optionParser.add_option("-c", "--checkerInstallation", dest="checkerInstallation", help="checker installation to use for post-installation checks", metavar="CHECKERINSTALLATION" )
(options, args) = optionParser.parse_args()

try:
    configpath = utils.makeAbsolutePath( args[0], os.getcwd() )
except IndexError:
    optionParser.print_usage( sys.stderr )
    sys.exit( 1 )

config = ConfigParser.SafeConfigParser()
config.read( configpath )

#make an unary functor to create absolute paths
abspath = partial( utils.makeAbsolutePath, relativeTo=os.path.dirname( configpath ) )

vmrun = utils.get_config_option( config, options.vmrun, "vmrun", utils.findVMRun() )
useGui = utils.get_config_option( config, None, "gui", "true" ).lower() == "true"
createErrorSnapshots = utils.get_config_option( config, None, "createErrorSnapshots", "true" ).lower() == "true"
hostType = utils.get_config_option( config, None, "type", "", "Host" )
hostLocation = utils.get_config_option( config, None, "location", "", "Host" )
hostUsername = utils.get_config_option( config, None, "username", "", "Host" )
hostPassword = utils.get_config_option( config, None, "password", "", "Host" )

cdashHost = utils.get_config_option( config, None, "host", "", "CDash" )
cdashLocation = utils.get_config_option( config, None, "location", "", "CDash" )
cdashProject = utils.get_config_option( config, None, "project", "", "CDash" )

if vmrun == None:
    die( "Could not find vmrun executable. Please specify it in the config file (vmrun=...) or via the --vmrun option" )

checkerInstallation = utils.get_config_option( config, options.checkerInstallation, "checkerInstallation" )
if checkerInstallation == None:
    die( "Could not find checker installation. Please specify it in the config file (checkerInstallation=...) or via the --checkerInstallation option" )

#apply functor to list to get absolute paths:
testcases = map( abspath, utils.get_enumerated_config_option( config, 'testcase' ) )
if len( testcases ) == 0:
    die( "No testcases specified. Please specify at least one test case in the configuration" )

vms = map( abspath, utils.get_enumerated_config_option( config, 'vm' ) )
if len( vms ) == 0:
    die( "No VMs specified. Please specify at least one VM in the configuration" )
    
installers = args[1:]

if len( installers ) > 0:
    source = source.Source()
    for i in installers:
        source.addDummy( 5, i )
else:
    source = ftpsource.FtpSource()
    if options.since:
        try:
            sdt = datetime.datetime.strptime( options.since, '%Y-%m-%d' )
        except ValueError:
            sdt = datetime.datetime.strptime( options.since, '%Y-%m-%d-%H-%M' )
        source.setStartDate( sdt )
    found = True
    nextsec = 0
    while found:
        sec = "Source{0}".format( nextsec )
        nextsec += 1
        try:
            host = config.get( sec, "host" )
            path = config.get( sec, "path" )
            platform = config.get( sec, "platform" )
            print( "** Add FTP location {0}:{1} ({2})".format( host, path, platform ) )
            source.addLocation( host, path, platform )
        except ConfigParser.NoSectionError:
            found = False
             
cdashHost = utils.get_config_option( config, None, "host", "", "CDash" )
cdashLocation = utils.get_config_option( config, None, "location", "", "CDash" )
cdashProject = utils.get_config_option( config, None, "project", "", "CDash" )

if len( cdashHost ) > 0:
    reporter = cdashreporter.CDashReporter( cdashHost, cdashLocation, cdashProject )
else:
    reporter = reporter.Reporter()
    
control = control.Control( vmrun, checkerInstallation, source, reporter )
control.setGuiEnabled( useGui )
control.setCreateErrorSnapshots( createErrorSnapshots )

if len( hostType ) > 0:
    control.setRemoteHost( hostType, hostLocation, hostUsername, hostPassword )

for i in vms:
    control.addVM( i )
for i in testcases:
    control.addTestCase( i )

control.run()
