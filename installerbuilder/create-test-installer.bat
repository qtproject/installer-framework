IF "%1" EQU "" (
  set OFFLINE_INSTALLER=true
  set ONLINE_INSTALLER=true
  set REPOGEN=true
  set TEST_ONLINE_INSTALLER=false
  set TEST_OFFLINE_INSTALLER=false
) else (
  set OFFLINE_INSTALLER=false
  set ONLINE_INSTALLER=false
  set REPOGEN=false
  set TEST_ONLINE_INSTALLER=false
  set TEST_OFFLINE_INSTALLER=false
)

for %%i in (%1,%2,%3,%4,%5,%6,%7,%8,%9) DO (
    IF "%%i" EQU "offline" (
      set OFFLINE_INSTALLER=true
    )
    IF "%%i" EQU "online" (
      set ONLINE_INSTALLER=true
    )
    IF "%%i" EQU "repogen" (
      set REPOGEN=true
    )
    IF "%%i" EQU "test_online" (
      set TEST_ONLINE_INSTALLER=true
    )
    IF "%%i" EQU "test_offline" (
      set TEST_OFFLINE_INSTALLER=true
    )
)

set AUTO_INSTALLATION_SCRIPT=--script %CD%\auto_installations_script.qs

set LOCAL_REPOSITORY_PATH=file:///%CD%\bin\repository
set LOCAL_REPOSITORY_PATH=%LOCAL_REPOSITORY_PATH:\=/%

call BatchSubstitute.bat http://www.xxxx.com/repository %LOCAL_REPOSITORY_PATH% ..\examples\testapp\config\config.xml > ..\examples\testapp\config\config.xml_new

copy /Y ..\examples\testapp\config\config.xml ..\examples\testapp\config\config.xml_old
move /Y ..\examples\testapp\config\config.xml_new ..\examples\testapp\config\config.xml

IF "%OFFLINE_INSTALLER%" EQU "true" (
  echo create offline installer
  bin\binarycreator -t bin\installerbase.exe -v -p ..\examples\testapp\packages -c ..\examples\testapp\config --offline-only bin\test-installer-offline.exe com.nokia.testapp
  IF errorlevel 1 pause ELSE echo ...done
)

IF "%ONLINE_INSTALLER%" EQU "true" (
  echo create online installer
  bin\binarycreator -t bin\installerbase.exe -v -n -p ..\examples\testapp\packages -c ..\examples\testapp\config bin\test-installer-online.exe com.nokia.testapp
  IF errorlevel 1 pause ELSE echo ...done
)

IF "%REPOGEN%" EQU "true" (
  echo create online repository
  IF exist bin\repository rmdir /S /Q bin\repository
  bin\repogen.exe -p ..\examples\testapp\packages -c ..\examples\testapp\config bin\repository com.nokia.testapp
  IF errorlevel 1 pause ELSE echo ...done
)

IF "%TEST_OFFLINE_INSTALLER%" EQU "true" (
  bin\test-installer-offline.exe --verbose %AUTO_INSTALLATION_SCRIPT%
)

IF "%TEST_ONLINE_INSTALLER%" EQU "true" (
  bin\test-installer-online.exe --verbose %AUTO_INSTALLATION_SCRIPT%
)

copy /Y ..\examples\testapp\config\config.xml_old ..\examples\testapp\config\config.xml
