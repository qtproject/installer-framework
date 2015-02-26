# -*- coding: utf-8 -*-
#!/usr/bin/env python
#############################################################################
##
## Copyright (C) 2015 The Qt Company Ltd.
## Contact: http://www.qt.io/licensing/
##
## This file is part of the Qt Installer Framework.
##
## $QT_BEGIN_LICENSE:LGPL$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see http://qt.io/terms-conditions. For further
## information use the contact form at http://www.qt.io/contact-us.
##
## GNU Lesser General Public License Usage
## Alternatively, this file may be used under the terms of the GNU Lesser
## General Public License version 2.1 or version 3 as published by the Free
## Software Foundation and appearing in the file LICENSE.LGPLv21 and
## LICENSE.LGPLv3 included in the packaging of this file. Please review the
## following information to ensure the GNU Lesser General Public License
## requirements will be met: https://www.gnu.org/licenses/lgpl.html and
## http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
##
## As a special exception, The Qt Company gives you certain additional
## rights. These rights are described in The Qt Company LGPL Exception
## version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
##
##
## $QT_END_LICENSE$
##
#############################################################################


from testexception import TestException

import fnmatch, hashlib, os

def md5sum( fileObj ):
    md5 = hashlib.md5()
    while True:
        chunk = fileObj.read( 4096 )
        if not chunk:
            break
        md5.update( chunk )
    return md5.hexdigest()

def locateFiles( rootPath, pattern ):
    for path, dirs, files in os.walk( os.path.abspath( rootPath ) ):
        for filename in fnmatch.filter( files, pattern ):
            yield os.path.join( path, filename )
            
def checkFileImpl( path, expectedSize=-1, expectedMd5=None ):
    #TODO: normalize path/convert to platform
    if not os.path.exists( path ):
        raise TestException( '{0}: file does not exist'.format( path ) )
    size = os.path.getsize( path )
    if expectedSize >= 0 and size != expectedSize:
        raise TestException( '{0}: unexpected size. Actual: {1} Expected: {2}'.format( path, size, expectedSize ) )
    if ( expectedMd5 != None ):
        fileObj = file( path, 'rb' )
        md5 = md5sum( fileObj )
        fileObj.close()
        if md5 != expectedMd5:
            raise TestException( '{0}: md5sum mismatch. Actual: {1} Expected: {2}'.format( path, md5, expectedMd5 ) )

