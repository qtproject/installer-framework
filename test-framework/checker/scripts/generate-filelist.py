#!/usr/bin/python
import optparse, os, sys
from testrunner import files, testrunner

class GenerateException( Exception ):
  def __init__( self, value ):
      self.value = value
  def __str__( self ):
      return repr( self.value )

def relpath( path, prefix ):
    if prefix != None:
        return os.path.relpath( path, prefix )
    else:
        return path;

out = sys.stdout

def walker( prefix, current_dir, children ):
    for c in children:
        child = current_dir + os.sep + c
        if os.path.isdir( child ):
            continue
        fileObj = file( child, 'rb' )
        md5 = files.md5sum( fileObj )
        out.write( "{0}; {1}; {2}\n".format( relpath( child, prefix ), os.path.getsize( child ), md5 ) )

optionParser = optparse.OptionParser(usage="%prog [options] directory", version="%prog 0.1")
optionParser.add_option("-p", "--omit-prefix", dest="prefix", help="make entries relative to this prefix", metavar="PREFIX" )
optionParser.add_option("-o", "--output", dest="output", help="save file list to file (instead of stdout)", metavar="OUTPUT" )
(options, args) = optionParser.parse_args()
     
try:
    directory = args[0]
except IndexError:
    raise GenerateException( "No directory given.")

if options.output != None:
    out = file( options.output, 'wb' )

os.path.walk( directory, walker, options.prefix )
