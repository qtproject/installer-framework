#!/usr/bin/env python
import sys, os, ConfigParser, optparse, time, virtualmachine, utils

# TODO: - Look in sensible locations for python in the guest VM
#       - Try and work-around VMware Fusion needing to be closed


def check_option( option, optionName ):
    if option:
        return
    print("** Could not find a value for {0}".format(optionName) )
    print("** Please specify it on the commandline or configuration file.")
    print
    optionParser.print_help();
    sys.exit(-1)

def die(message):
    print "** " + str(message)
    sys.exit(1);

#
# Parse options
#

optionParser = optparse.OptionParser(usage="%prog [options] vmx-file", version="%prog 0.9")
optionParser.add_option("-u", "--username", dest="username", help="username for VM", metavar="USERNAME" )
optionParser.add_option("-p", "--password", dest="password", help="password for VM", metavar="PASSWORD" )
optionParser.add_option("-S", "--script", dest="script", help="script for VM", metavar="SCRIPT" )
optionParser.add_option("-s", "--snapshot", dest="snapshot", help="snapshot for VM", metavar="SNAPSHOT" )
optionParser.add_option("-P", "--python", dest="python", help="python location in VM", metavar="PYTHON" )
optionParser.add_option("-v", "--vmrun", dest="vmrun", help="vmrun command for VM", metavar="VMRUN")
optionParser.add_option("-c", "--config", dest="config", help="configuration file for VM", metavar="CONFIG")
optionParser.add_option("-i", "--installer", dest="installer", help="installer executable to command inside the VM", metavar="INSTALLER")
optionParser.add_option("-e", "--installscript", dest="installscript", help="QtScript script to use for non-interactive installation", metavar="INSTALLSCRIPT")

(options, args) = optionParser.parse_args()

try:
  options.vmx = args[0]
except IndexError:
  options.vmx = None

options.ostype = None
options.guestTempDir = None

config = ConfigParser.SafeConfigParser()
if options.config:
    print("** Reading config: " + options.config )
    config.read( options.config )
else:
    print("** No config given")
    
options.username = utils.get_config_option( config, options.username, "username", "nokia" )
options.password = utils.get_config_option( config, options.password, "password", "nokia" )
options.script = utils.get_config_option( config, options.script, "script", "guest.py" )
options.python = utils.get_config_option( config, options.python, "python", "c:/python26/python.exe" )
options.snapshot = utils.get_config_option( config, options.snapshot, "snapshot", "base" )
options.vmrun = utils.get_config_option( config, options.vmrun, "vmrun" )
options.vmx = utils.get_config_option( config, options.vmx, "vmx" )
options.guestTempDir = utils.get_config_option( config, options.guestTempDir, "tempDir", "c:\\windows\\temp" )
options.ostype = utils.get_config_option( config, options.ostype, "os", "windows" )

check_option( options.vmx, "vmx-file" )

# Search the PATH and the a few extra locations for 'vmrun'
if not options.vmrun:
    options.vmrun = utils.findVMRun()

check_option( options.vmrun, "VMRUN" )
check_option( options.installer, "INSTALLER" )
check_option( options.installscript, "INSTALLSCRIPT" )

#
# VM actions
#

vm = virtualmachine.VirtualMachine( options.vmrun, options.vmx, options.username, options.password, options.guestTempDir, options.ostype )

snapshotExists = vm.snapshotExists( options.snapshot )
if not snapshotExists:
    die("Could not find '{0}' snapshot, please create it in the VM.".format( options.snapshot ) )

revertStatus, _ = vm.revertToSnapshot( options.snapshot )
if revertStatus != 0:
    die("Failed to revert to snapshot")

time.sleep( 5 ) # Trying to avoid a possible race between restore and start

vm.start()

try:
    pythonStatus, _ = vm.command("Checking for guest installed Python", "fileExistsInGuest", options.python)
    if pythonStatus != 0:
        raise virtualmachine.VMException("Please install Python in the VM from http://www.python.org/download/")

    wrapperpath = vm.copyToTemp( 'guest.py' )
    installerpath = vm.copyToTemp( options.installer )
    scriptpath = vm.copyToTemp( options.installscript )
    outputpath = vm.mkTempPath( 'output.log' )
    vm.command( 'Execute installer', "runProgramInGuest", "-interactive -activeWindow '{0}' '{1}' '{2}' '{3}' --script '{4}'".format( options.python, wrapperpath, outputpath, installerpath, scriptpath ) )
    
    vm.copyFromTemp( 'output.log', 'output.log' )
    result = ConfigParser.SafeConfigParser()
    result.read( 'output.log' )
    print( "Installer exit code: " + result.get( 'Result', 'ExitCode' ) )
    #TODO parse output and do something with it
    
except virtualmachine.VMException as exception:
    die( exception )
finally:
    vm.kill()
