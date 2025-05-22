@echo on
SetLocal EnableDelayedExpansion
REM set msvc env
CALL "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64

REM set env path
set path=%path%;%~dp0..\package\qt-static-6.6.0\bin
set path=%path%;%~dp0..\package\jom

REM qmake
qmake -v
qmake .\installerfw.pro -spec win32-msvc "CONFIG+=qtquickcompiler"  -o %~dp0..\build_output

REM jom
cd build_output
jom.exe clean
jom.exe qmake_all
jom.exe -j4 -f Makefile
EndLocal