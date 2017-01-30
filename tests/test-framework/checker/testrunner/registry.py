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


from testexception import TestException
import _winreg

_registry = dict()
_registry["HKEY_CLASSES_ROOT"] = _winreg.HKEY_CLASSES_ROOT
_registry["HKEY_CURRENT_USER"] = _winreg.HKEY_CURRENT_USER
_registry["HKEY_LOCAL_MACHINE"] = _winreg.HKEY_LOCAL_MACHINE
_registry["HKEY_USERS"] = _winreg.HKEY_USERS
_registry["HKEY_CURRENT_CONFIG"] = _winreg.HKEY_CURRENT_CONFIG

def splitKey( key ):
    key, separator, subKey = key.partition( '\\' )
    return _registry[key], subKey
    
def checkKey( key, value, expectedData ):
    baseKey, subKey = splitKey( key )
    keyHandle = _winreg.OpenKey( baseKey, subKey )
    data, _ = _winreg.QueryValueEx( keyHandle, value )
    if data != expectedData:
        raise TestException( '{0}: unexpected registry data. Actual: {1} Expected: {2}'.format( key, data, expectedData ) )
