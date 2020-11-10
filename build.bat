@ECHO ON

SET CLEAN=%1

if "%TOOLS_PREFIX%"=="" (
  SET TOOLS_PREFIX=C:
)

SET JOM=%TOOLS_PREFIX%\jom\jom.exe
SET QMAKE=%TOOLS_PREFIX%\Qt_static\v5.12.7\bin\qmake.exe
SET WIN_SDK=10.0.17763.0

if "%CLEAN%"=="" call clean.bat 1
call "C:\%BUILDTOOLS_PREFIX%\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 %WIN_SDK%
call create_framework.bat
if "%CLEAN%"=="" call clean.bat 0