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
  ..\..\bin\repogen.exe -p ..\..\examples\testapp\packages -c ..\..\examples\testapp\config\config.xml ..\..\bin\repository
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

