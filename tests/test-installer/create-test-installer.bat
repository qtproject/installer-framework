:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::
:: Copyright (C) 2015 The Qt Company Ltd.
:: Contact: http://www.qt.io/licensing/
::
:: This file is part of the Qt Installer Framework.
::
:: $QT_BEGIN_LICENSE:LGPL$
:: Commercial License Usage
:: Licensees holding valid commercial Qt licenses may use this file in
:: accordance with the commercial license agreement provided with the
:: Software or, alternatively, in accordance with the terms contained in
:: a written agreement between you and The Qt Company. For licensing terms
:: and conditions see http://qt.io/terms-conditions. For further
:: information use the contact form at http://www.qt.io/contact-us.
::
:: GNU Lesser General Public License Usage
:: Alternatively, this file may be used under the terms of the GNU Lesser
:: General Public License version 2.1 or version 3 as published by the Free
:: Software Foundation and appearing in the file LICENSE.LGPLv21 and
:: LICENSE.LGPLv3 included in the packaging of this file. Please review the
:: following information to ensure the GNU Lesser General Public License
:: requirements will be met: https://www.gnu.org/licenses/lgpl.html and
:: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
::
:: As a special exception, The Qt Company gives you certain additional
:: rights. These rights are described in The Qt Company LGPL Exception
:: version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
::
::
:: $QT_END_LICENSE$
::
:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

@IF "%1" EQU "" (
  @set OFFLINE_INSTALLER=true
  @set ONLINE_INSTALLER=true
  @set REPOGEN=true
  @set TEST_ONLINE_INSTALLER=false
  @set TEST_OFFLINE_INSTALLER=false
) else (
  @set OFFLINE_INSTALLER=false
  @set ONLINE_INSTALLER=false
  @set REPOGEN=false
  @set TEST_ONLINE_INSTALLER=false
  @set TEST_OFFLINE_INSTALLER=false
)

@for %%i in (%1,%2,%3,%4,%5,%6,%7,%8,%9) DO (
    @IF "%%i" EQU "offline" (
      @set OFFLINE_INSTALLER=true
    )
    @IF "%%i" EQU "online" (
      @set ONLINE_INSTALLER=true
    )
    @IF "%%i" EQU "repogen" (
      @set REPOGEN=true
    )
    @IF "%%i" EQU "test_online" (
      @set TEST_ONLINE_INSTALLER=true
    )
    @IF "%%i" EQU "test_offline" (
      @set TEST_OFFLINE_INSTALLER=true
    )
)

@set AUTO_INSTALLATION_SCRIPT=--script %CD%\auto_installations_script.qs

@set BINARY_PATH_RELATIVE=%CD%\..\..\bin
@pushd .
@cd %BINARY_PATH_RELATIVE%
@set BINARY_PATH_ABSOLUTE=%CD%
@popd

@set LOCAL_REPOSITORY=file:///%BINARY_PATH_ABSOLUTE%/repository
@set LOCAL_REPOSITORY_PATH=%LOCAL_REPOSITORY:\=/%

call BatchSubstitute.bat http://www.xxxx.com/repository %LOCAL_REPOSITORY_PATH% ..\..\examples\testapp\config\config.xml > ..\..\examples\testapp\config\config.xml_new
@if %ERRORLEVEL% NEQ 0 goto error_marker

copy /Y ..\..\examples\testapp\config\config.xml ..\..\examples\testapp\config\config.xml_old
@if %ERRORLEVEL% NEQ 0 goto error_marker

move /Y ..\..\examples\testapp\config\config.xml_new ..\..\examples\testapp\config\config.xml
@if %ERRORLEVEL% NEQ 0 goto error_marker

IF "%OFFLINE_INSTALLER%" EQU "true" (
  echo create offline installer
  ..\..\bin\binarycreator -t ..\..\bin\installerbase.exe -v -p ..\..\examples\testapp\packages -c ..\..\examples\testapp\config\config.xml --offline-only ..\..\bin\test-installer-offline.exe
  @if %ERRORLEVEL% NEQ 0 goto error_marker ELSE goto done_marker
)

IF "%ONLINE_INSTALLER%" EQU "true" (
  echo create online installer
  ..\..\bin\binarycreator -t ..\..\bin\installerbase.exe -v -n -p ..\..\examples\testapp\packages -c ..\..\examples\testapp\config\config.xml ..\..\bin\test-installer-online.exe
  @if %ERRORLEVEL% NEQ 0 goto error_marker ELSE goto done_marker
)

IF "%REPOGEN%" EQU "true" (
  echo create online repository
  @IF exist ..\..\bin\repository rmdir /S /Q ..\..\bin\repository
  ..\..\bin\repogen.exe -p ..\..\examples\testapp\packages ..\..\bin\repository
  @if %ERRORLEVEL% NEQ 0 goto error_marker ELSE goto done_marker
)

@IF "%TEST_OFFLINE_INSTALLER%" EQU "true" (
  ..\..\bin\test-installer-offline.exe --verbose %AUTO_INSTALLATION_SCRIPT%
  @if %ERRORLEVEL% NEQ 0 goto error_marker ELSE goto done_marker
)

@IF "%TEST_ONLINE_INSTALLER%" EQU "true" (
  ..\..\bin\test-installer-online.exe --verbose %AUTO_INSTALLATION_SCRIPT%
  @if %ERRORLEVEL% NEQ 0 goto error_marker ELSE goto done_marker
)

copy /Y ..\..\examples\testapp\config\config.xml_old ..\..\examples\testapp\config\config.xml
@if %ERRORLEVEL% NEQ 0 goto error_marker

goto done_marker

:error_marker
echo *** Error compiling ifw ***
pause

:done_marker
echo ...done

