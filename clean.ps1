param
(
    [switch]$SkipBuildDir,
    [switch]$DeepClean,
    [switch]$Verbose
)

function RemoveDir ($dir) {
    if (Test-Path "$PSScriptRoot\$dir") {
        Remove-Item "$PSScriptRoot\$dir" -Force -Recurse
        if ($Verbose) { Write-Output " -> d: $dir" }
    }
}

function RemoveFile ($file) {
    if (Test-Path "$PSScriptRoot\$file") {
        Remove-Item "$PSScriptRoot\$file" -Force
        if ($Verbose) { Write-Output " -> f: $file" }
    }
}

if ($Verbose) { Write-Output "Cleanup starting" }
if ($Verbose) { Write-Output "Removing the following (f)iles and (d)irectories:" }

if (!$SkipBuildDir) { RemoveDir "build" }

RemoveDir "bin"

$baseFiles = ".qmake.stash",
    "src\libs\7zip\mocinclude.opt",
    "src\libs\installer\mocinclude.opt",
    "src\sdk\installerbase.qrc",
    "src\sdk\mocinclude.opt",
    "src\sdk\translations\ifw_en.ts",
    "tools\archivegen\mocinclude.opt",
    "tools\binarycreator\mocinclude.opt",
    "tools\devtool\mocinclude.opt",
    "tools\repocompare\mocinclude.opt",
    "tools\repogen\mocinclude.opt"

foreach ($file in $baseFiles) {
    RemoveFile $file
}

if ($DeepClean) {
    if ($Verbose) {
        Write-Output "---"
        Write-Output "Removing additional (f)iles and (d)irectories since -DeepClean was provided:"
    }

    $sources = "libs\7zip", "libs\installer", "sdk", "sdk\translations"
    $tools = "archivegen", "binarycreator", "devtool", "repocompare", "repogen"
    $alldirs = @{ "src" = $sources; "tools" = $tools }

    foreach ($pair in $alldirs.GetEnumerator()) {
        RemoveFile "$($pair.Name)\Makefile"
        foreach ($subDir in ($pair.Value)) {
            # Dirs
            RemoveDir "$($pair.Name)\$subDir\debug"
            RemoveDir "$($pair.Name)\$subDir\release"

            # Files
            RemoveFile "$($pair.Name)\$subDir\Makefile"
            RemoveFile "$($pair.Name)\$subDir\Makefile.Debug"
            RemoveFile "$($pair.Name)\$subDir\Makefile.Release"

            if (($pair.Name) -eq "tools") {
                RemoveFile "$($pair.Name)\$tool\${tool}_plugin_import.cpp"
            }
        }
    }

    RemoveDir "lib"

    $additionalFiles = "src\libs\Makefile",
        "Makefile",
        "src\libs\installer\ui_authenticationdialog.h",
        "src\libs\installer\ui_proxycredentialsdialog.h",
        "src\libs\installer\ui_serverauthenticationdialog.h",
        "src\sdk\installerbase_plugin_import.cpp",
        "src\sdk\ui_settingsdialog.h",
        "tools\repocompare\ui_mainwindow.h"

    foreach ($file in $additionalFiles) {
        RemoveFile $file
    }
}

if ($Verbose) { Write-Output "---" }
if ($Verbose) { Write-Output "Cleanup completed" }
