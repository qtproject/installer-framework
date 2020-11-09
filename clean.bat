@ECHO ON

SET REMOVE_BUILD_DIR=%1

IF %REMOVE_BUILD_DIR% NEQ 0 (
  IF EXIST build RMDIR /Q /S build
)

IF EXIST bin RMDIR /Q /S bin
IF EXIST .qmake.stash DEL .qmake.stash
IF EXIST src\libs\7zip\mocinclude.opt DEL src\libs\7zip\mocinclude.opt
IF EXIST src\libs\installer\mocinclude.opt DEL src\libs\installer\mocinclude.opt
IF EXIST src\sdk\installerbase.qrc DEL src\sdk\installerbase.qrc
IF EXIST src\sdk\mocinclude.opt DEL src\sdk\mocinclude.opt
IF EXIST src\sdk\translations\ifw_en.ts DEL src\sdk\translations\ifw_en.ts
IF EXIST tools\archivegen\mocinclude.opt DEL tools\archivegen\mocinclude.opt
IF EXIST tools\binarycreator\mocinclude.opt DEL tools\binarycreator\mocinclude.opt
IF EXIST tools\devtool\mocinclude.opt DEL tools\devtool\mocinclude.opt
IF EXIST tools\repocompare\mocinclude.opt DEL tools\repocompare\mocinclude.opt
IF EXIST tools\repogen\mocinclude.opt DEL tools\repogen\mocinclude.opt
