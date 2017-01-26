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

import datetime, socket, sys, traceback
from xml.sax.saxutils import XMLGenerator
from xml.sax.xmlreader import AttributesNSImpl
import result
from result import exitStatusAsString
from xmlutils import startElement, endElement, writeElement

class Reporter:
    def __init__( self ):
        pass
    
    def reportException( self ):
        sys.stderr.write( traceback.format_exc( 15 ) + '\n' )
        
    def reportResult( self, result ):
        try:
            self.toXml( result, sys.stdout )
        finally:
            sys.stdout.flush()
        
    def toXml( self, result, out ):
        atom = "http://www.w3.org/2005/Atom"
        tf = "http://sdk.nokia.com/test-framework/ns/1.0"
        
        gen = XMLGenerator( out, 'utf-8' )
        gen.startDocument()
        gen.startPrefixMapping( 'atom', atom)
        gen.startPrefixMapping( 'tf', tf )

        startElement( gen, atom, 'entry' )
        
        writeElement( gen, atom, 'title', result.constructTitle() )
        writeElement( gen, atom, 'updated', datetime.datetime.now().isoformat() )

        writeElement( gen, tf, 'errorSummary', exitStatusAsString( result.status() ) )

        writeElement( gen, tf, 'host', socket.gethostname() )
        if result._testStart != None:
            writeElement( gen, tf, 'testStart', result._testStart.isoformat() )
        else:
            result._internalErrors.append( "Result generator: no start timestamp found." )

        if result._testEnd != None:
            writeElement( gen, tf, 'testEnd', result._testEnd.isoformat() )
        else:
            result._internalErrors.append( "Result generator: no end timestamp found." )
            
        
        startElement( gen, tf, 'installer' )
        writeElement( gen, tf, 'sourceUrl', result._installerSourceLocation )
        writeElement( gen, tf, 'platform', result._installerTargetPlatform )
        #TODO revision
        endElement( gen, tf, 'installer' )

        if result._testcase != None:
            startElement( gen, tf, 'testCase' )
            writeElement( gen, tf, 'name', result._testcase.name() )
            writeElement( gen, tf, 'path', result._testcase.path() )
            writeElement( gen, tf, 'installScript', result._testcase.installscript() )
            endElement( gen, tf, 'testCase' )
        else:
            result._internalErrors.append( "Result generator: No test case given." )

        if result._installationResult != None:
            startElement( gen, tf, 'installationResult' )
            writeElement( gen, tf, 'exitCode', str( result._installationResult.exitCode ) )    
            writeElement( gen, tf, 'exitStatus', exitStatusAsString( result._installationResult.exitStatus ) )    
            endElement( gen, tf, 'installationResult' )
        else:
            result._internalErrors.append( "Result generator: No installation result given." )
        startElement( gen, tf, 'checkerResult' )
        
        for err in result._checkerErrors:
            writeElement( gen, tf, 'error', err )    
        endElement( gen, tf, 'checkerResult' )
            
        startElement( gen, tf, 'virtualMachine' )
        writeElement( gen, tf, 'path', result._vm.vmxPath() )
        writeElement( gen, tf, 'platform', result._vm.ostype() )
        writeElement( gen, tf, 'snapshot', result._vm.snapshot() )
        endElement( gen, tf, 'virtualMachine' )

        startElement( gen, tf, 'internalErrors' )
        for i in result._internalErrors:
            writeElement( gen, tf, 'internalError', str( i ) )

        endElement( gen, tf, 'internalErrors' )
        
        if result._errorSnapshot != None:
            writeElement( gen, tf, 'errorSnapshot', result._errorSnapshot )
            
        endElement( gen, atom, 'entry' )
        
        gen.endDocument()

 
