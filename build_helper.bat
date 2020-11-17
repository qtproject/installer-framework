SETLOCAL

CD /d %~dp0

SET VARSALL=%1
SET WINSDK=%2
SET QMAKE=%3
SET JOM=%4

rem Set up the required cmd environment
call %VARSALL% x86 %WINSDK%

rem Create the makefiles
%QMAKE% -r

rem Build the makefiles
%JOM% release

ENDLOCAL