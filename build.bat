@ECHO ON

SET CLEAN=%1

if "%JOM%"=="" (
  SET JOM=C:\Qt\Tools\QtCreator\bin\jom.exe
)

if "%QMAKE_PREFIX%"=="" (
    SET QMAKE_PREFIX=C:
)

SET QMAKE=%QMAKE_PREFIX%\Qt_static\v5.12.7\bin\qmake.exe
SET WIN_SDK=10.0.17763.0

if "%CLEAN%"=="" call clean.bat 1
call "C:\%BUILDTOOLS_PREFIX%\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 %WIN_SDK%
call create_framework.bat
if "%CLEAN%"=="" call clean.bat 0