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


import datetime, socket, os, sys, traceback, time, tempfile, httplib, urllib
from xml.sax.saxutils import XMLGenerator
from xml.sax.xmlreader import AttributesNSImpl
import result, utils
from result import exitStatusAsString
from xmlutils import startElement, endElement, writeElement

checkerResultAsString = { result.CheckerResult.Passed:u"passed", result.CheckerResult.Failed:u"failed", result.CheckerResult.NotRun:u"notRun" }

def formatDate( date ):
    return date.strftime( '%Y%m%d-%H%M' )

def writeExecutionResult( gen, name, r ):
        if r == None or r.hasError():
            stat = "failed"
        else:
            stat = "passed"

        startElement( gen, None, "Test", { u"Status":stat } )
        writeElement( gen, None, "Name", name )
        writeElement( gen, None, "FullName", name )
        startElement( gen, None, "NamedMeasurement", { u"type":u"text/string", u"name":u"Exit Code"} )
        if r:
            msg = "(Unexpected)"
            ec = r.exitCode           
            if ec == 0:
               msg = "(Success)"
            elif ec == 1:
               msg = "(Failed)"
            elif ec == 2:
               msg = "(Canceled)"
  
            writeElement( gen, None, "Value", "Exit status: {0}; Installer exit code: {1} {2}".format( exitStatusAsString( r.exitStatus ), str( r.exitCode ), msg ) )
        else:
            writeElement( gen, None, "Value", "Could not determine installation result." )

        endElement( gen, None, "NamedMeasurement" )


        if r:
            startElement( gen, None, "NamedMeasurement", { u"type":u"numeric/double", u"name":u"Execution Time"} )
            writeElement( gen, None, "Value", str( r.executionTime ) )
            endElement( gen, None, "NamedMeasurement" )


        endElement( gen, None, "Test" )

def writeInternalError( gen, errorstr, num ):
        startElement( gen, None, "Test", { u"Status":u"failed"} )
        name = "InternalError{0}".format( num )
        writeElement( gen, None, "Name", name )
        writeElement( gen, None, "FullName", name )
        startElement( gen, None, "NamedMeasurement", { u"type":u"text/string", u"name":u"Completion Status"} )
        writeElement( gen, None, "Value", errorstr )
        endElement( gen, None, "NamedMeasurement" )
        endElement( gen, None, "Test" )
      
def writeTest( gen, test ):
        startElement( gen, None, "Test", { u"Status":checkerResultAsString[test.result]} )
        writeElement( gen, None, "Name", test.name )
        writeElement( gen, None, "FullName", test.name )
        startElement( gen, None, "NamedMeasurement", { u"type":u"text/string", u"name":u"Completion Status"} )
        writeElement( gen, None, "Value", test.errorString )
        endElement( gen, None, "NamedMeasurement" )
        endElement( gen, None, "Test" )
    
class CDashReporter:
    def __init__( self, host, location, project ):
        self._host = host
        self._location = location
        self._project = project
        
    def reportException( self ):
        sys.stderr.write( traceback.format_exc( 15 ) + '\n' )
        #TODO

    def reportResult( self, result ):
        self._buildStamp = '{0}-Nightly'.format( datetime.datetime.now().strftime( '%Y%m%d-%H%M' ), utils.randomString( 4 ) )
        try:
            #generate XML
            self.genXml( self.genTestXml, result )
            #generate Tests.xml
            #submitFile('Tests.xml')
        except:
            raise

    def genTestXml( self, gen, result ):
        name = "{0} on {1}".format( result._installer.sourceFilename, result._vm.name() )
        startElement( gen, None, "Site", { u"BuildStamp":self._buildStamp, u"Name":name, u"Generator":u"VMTester 0.1" } )
        startElement( gen, None, "Testing" )
        writeElement( gen, None, "StartDateTime", formatDate( result._testStart ) )
        startElement( gen, None, "TestList" )
        internal = result._internalErrors
        for i in range( 0, len( internal ) - 1 ):
            writeElement( gen, None, "Test", "InternalError{0}".format( i ) )

        for stepNum in range( len( result._stepResults ) ):
            step = result._stepResults[stepNum]
            writeElement( gen, None, "Test", "installer-run{0}".format( stepNum ) )
            for i in step.checkerResults:
                writeElement( gen, None, "Test", i.name )              
        endElement( gen, None, "TestList" )
        
        for i in range( 0, len( internal ) - 1 ):
            writeInternalError( gen, internal[i], i )
        for stepNum in range( len( result._stepResults ) ):
            step = result._stepResults[stepNum]    
            writeExecutionResult( gen, "installer-run{0}".format( stepNum ), step.executionResult )
            for i in step.checkerResults:
                writeTest( gen, i )
        endElement( gen, None, "Testing" )
        endElement( gen, None, "Site" )
        
    def genXml( self, fillFunc, result ):
        f = None
        try:
            f = tempfile.NamedTemporaryFile( delete=False )
            gen = XMLGenerator( f, 'utf-8' )
            gen.startDocument()
            fillFunc( gen, result )
            gen.endDocument()
            f.close()
            self.submitFile( f.name )
        finally:
            if f:
                os.unlink( f.name )
            
    def submitFile( self, path ):
        f = file( path )
        params = urllib.urlencode( {'project': self._project} )
        conn = None
        try:
            try:
                conn = httplib.HTTPConnection( self._host )
                conn.request( "POST", "{0}/submit.php?project={1}".format( self._location, self._project ), f )
                response = conn.getresponse()
                print response.status, response.reason
                #TODO check result
            finally:
                if conn:
                    conn.close()
        except:
            #TODO: if submitting to cdash fails, try to notify the admin (mail?)
            raise

if __name__ == "__main__":
    r = result.Result()
    r.testStarted()
    r.addCheckerResult( result.CheckerResult( "test1", result.CheckerResult.Passed, "" ) )
    r.addCheckerResult( result.CheckerResult( "test2", result.CheckerResult.Failed, "Something went wrong, dude!" ) )
    time.sleep( 1 )
    r.testFinished()
    cr = CDashReporter( "http://localhost", "/CDash", "test1" )
    cr.reportResult( r )

