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


from ftplib import FTP
import datetime, functools, os, time, tempfile
from source import Installer
import utils

def _timestampFromFilename( platform, fn ):
    # windows: assumed format is YYYY_mm_dd_HH_MM as prefix
    # linux: assumed format is YYYY-mm-dd as suffix
    if platform.startswith( 'windows' ):        
        format = '%Y_%m_%d_%H_%M'
        length = len( 'YYYY_mm_dd_HH_MM' )
        return datetime.datetime.strptime( fn[0:length], format )
    else:
        format = '%Y-%m-%d'
        length = len( 'YYYY-mm-dd' )
        return datetime.datetime.strptime( fn[-length:], format )
       


def timestampFromFilename( platform, fn ):
    try:
        return _timestampFromFilename( platform, fn )
    except ValueError:
        return None

def filesNewerThan( files, dt ):
    if dt:
        return filter( lambda (x,y): y >= dt, files )
    else:
        return files

#internal to FtpSource
class Location:
    def __init__( self, host, path, platform ):
        self.host = host
        self.path = path
        self.platform = platform
        self.testedFiles = []

    def ls( self ):
        ftp = FTP( self.host )
        ftp.login()
        try:
            return map( utils.basename, ftp.nlst( self.path ) )
        finally:
            ftp.close()

    def filesSortedByTimestamp( self ):
        files = self.ls()
        withTS = [( i, timestampFromFilename( self.platform, i ) ) for i in files]
        filtered = filter( lambda (x,y): y != None, withTS )
        filtered.sort( key=lambda (x,y): y )
        return filtered

    def untestedFilesSortedByTimestamp( self ):
        l = self.filesSortedByTimestamp()
        return filter( lambda ( x, y ): not self.isTested( x ), l )

    def markAsTested( self, filename ):
        self.testedFiles.append( filename )

    def isTested( self, filename ):
        return filename in self.testedFiles

    def description( self ):
        return "host={0} path={1} platform={2}".format( self.host, self.path, self.platform )

    def download( self, fn, target ):
        ftp = FTP( self.host )
        ftp.login()
        try:
             ftp.retrbinary( 'RETR {0}/{1}'.format( self.path, fn ), target.write )
        finally:
             ftp.close()

class FtpSource:
    def __init__( self, delay=60*60, tempdir='/tmp' ):
        self._locations = []
        self._delay = delay
        self._tempdir = tempdir
        self._startDate = None

    def setStartDate( self, date ):
        self._startDate = date

    def addLocation( self, host, path, platform ):
        self._locations.append( Location( host, path, platform ) )

    def nextInstaller( self ):
        while True:
            for i in self._locations:
                print( "** Checking FTP location: " + i.description() )
                files = i.untestedFilesSortedByTimestamp()
                if self._startDate != None:
                    files = filesNewerThan( files, self._startDate )
                if len( files ) == 0:
                    continue;
                fn, ts = files[0] 
                print( "** Downloading new installer: {0}...".format( fn ) )
                tmp = tempfile.NamedTemporaryFile( dir=self._tempdir, prefix=fn )
                i.download( fn, tmp )
                print( "** Download completed. ({0})".format( tmp.name ) )
                i.lastTested = ts
                inst = Installer( tmp.name, i.platform, i, ts, tmp )
                inst.sourceFilename = fn
                return inst
            print( "** No installers found. Going to sleep for {0} seconds...".format( self._delay ) )
            time.sleep( self._delay )

if __name__ == "__main__":
     src = FtpSource()
     src.addLocation( "hegel", "/projects/ndk/installers/windows", "windows" )
     while True:
         inst = src.nextInstaller()
         print inst
         inst.markAsTested()
