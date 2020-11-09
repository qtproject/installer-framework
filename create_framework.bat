@ECHO ON

MKDIR build

REM Create the makefiles
CALL %QMAKE% -r

REM Build the makefiles
CALL %JOM% release

REM Copy the output to the build folder
COPY bin\binarycreator.exe build\binarycreator.exe
COPY bin\installerbase.exe build\installerbase.exe
