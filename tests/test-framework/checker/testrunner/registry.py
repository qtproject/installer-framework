# -*- coding: utf-8 -*-
from testexception import TestException
import _winreg

_registry = dict()
_registry["HKEY_CLASSES_ROOT"] = _winreg.HKEY_CLASSES_ROOT
_registry["HKEY_CURRENT_USER"] = _winreg.HKEY_CURRENT_USER
_registry["HKEY_LOCAL_MACHINE"] = _winreg.HKEY_LOCAL_MACHINE
_registry["HKEY_USERS"] = _winreg.HKEY_USERS
_registry["HKEY_CURRENT_CONFIG"] = _winreg.HKEY_CURRENT_CONFIG

def splitKey( key ):
    key, seperator, subKey = key.partition( '\\' )
    return _registry[key], subKey
    
def checkKey( key, value, expectedData ):
    baseKey, subKey = splitKey( key )
    keyHandle = _winreg.OpenKey( baseKey, subKey )
    data, _ = _winreg.QueryValueEx( keyHandle, value )
    if data != expectedData:
        raise TestException( '{0}: unexpected registry data. Actual: {1} Expected: {2}'.format( key, data, expectedData ) )
