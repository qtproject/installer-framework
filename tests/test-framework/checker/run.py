# -*- coding: utf-8 -*-
import optparse, sys
from testrunner import testrunner

from xml.sax.saxutils import XMLGenerator
from xml.sax.xmlreader import AttributesNSImpl

class Writer:
    def __init__( self, out ):
        self._out = out
        self.gen = XMLGenerator( out, 'utf-8' )
        self.gen.startDocument()
        self.gen.startElement( 'results', {} )
    
    def addResult( self, name, status, errstr ):
        self.gen.startElement('result', { 'name':name, 'status':status } )
        self.gen.characters( errstr )
        self.gen.endElement('result' )

    def addFailed( self, name, errstr ):
        self.addResult( name, "failed", errstr )

    def addPassed( self, status, errstr ):
        self.addResult( name, "passed", errstr )
        
    def finalize( self ):
        self.gen.endElement( 'results' )
        self.gen.endDocument
        self._out.write( '\n' )

optionParser = optparse.OptionParser(usage="%prog [options] testcaseDir", version="%prog 0.1")
optionParser.add_option("-p", "--omit-prefix", dest="prefix", help="for file checks, prefix relative paths with this prefix", metavar="PREFIX" )
optionParser.add_option("-o", "--output", dest="output", help="write results to file (instead of stdout)", metavar="OUTPUT" )
(options, args) = optionParser.parse_args()

try:
    testdir = args[0]
except IndexError:
    optionParser.print_usage( sys.stderr )
    sys.exit( 1 )

out = sys.stdout
if options.output != None:
    out = file( options.output, 'wb' )

writer = Writer( out )
try:
    runner = testrunner.TestRunner( testdir, options.prefix, writer )
    runner.run()
finally:
    writer.finalize()