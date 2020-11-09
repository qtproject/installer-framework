@ECHO ON

SET JOM=C:\Qt\Tools\QtCreator\bin\jom.exe
SET QMAKE=C:\Qt_static\v5.12.7\bin\qmake.exe
SET WIN_SDK=10.0.17763.0

call clean.bat 1
call C:\BuildTools\VC\Auxiliary\Build\vcvarsall.bat x86 %WIN_SDK%
call create_framework.bat
call clean.bat 0
