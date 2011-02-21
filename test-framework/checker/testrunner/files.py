# -*- coding: utf-8 -*-
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

