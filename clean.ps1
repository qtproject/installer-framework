param
(
    [switch]$SkipBuildDir,
    [switch]$DeepClean,
    [switch]$Verbose
)

if ($Verbose) {
    $DebugPreference = 'Continue'
}

function RemoveDir ($dir) {
    if (Test-Path "$PSScriptRoot\$dir") {
        Remove-Item "$PSScriptRoot\$dir" -Force -Recurse
        Write-Debug " -> d: $dir"
    }
}

function RemoveFile ($file) {
    if (Test-Path "$PSScriptRoot\$file") {
        Remove-Item "$PSScriptRoot\$file" -Force
        Write-Debug " -> f: $file"
    }
}

Write-Debug "Cleanup starting"
Write-Debug "Removing the following (f)iles and (d)irectories:"

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

# Remove the built translation files
Get-ChildItem -Path "$PSScriptRoot\src\sdk\translations" *.qm | ForEach-Object { Remove-Item -Path $_.FullName -Force }

if ($DeepClean) {
    Write-Debug "---"
    Write-Debug "Removing additional (f)iles and (d)irectories since -DeepClean was provided:"

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

Write-Debug "---"
Write-Debug "Cleanup completed"

$DebugPreference = 'SilentlyContinue'
