param
(
    [switch]$DeepClean,
    [String]$ToolsPrefix = "C:",
    [String]$WinSdk = "10.0.17763.0",
    [String]$BuildToolsPrefix = "",
    [switch]$Verbose,
    [switch]$SkipCleaning,
    [switch]$SkipPreClean,
    [switch]$SkipPostClean
)

if ($Verbose) {
    $DebugPreference = 'Continue'
}

$DeepCleanSwitch = ""
if ($DeepClean) { $DeepCleanSwitch = "-DeepClean" }
$VerboseSwitch = ""
if ($Verbose) { $VerboseSwitch = "-Verbose" }

$jom = "$ToolsPrefix\jom\jom.exe"
$qmake = "$ToolsPrefix\static\qt\v5.12.7\bin\qmake.exe"
$varsall = "C:\$BuildToolsPrefix\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"

Write-Output "Start building installer framework"
Write-Output "---"

if (!(Test-Path $jom)) {
    Write-Output "Jom was not found at '$jom'! Exiting..."
    Return
} else {
    Write-Debug "Jom was found at '$jom'."
}
if (!(Test-Path $qmake)) {
    Write-Output "Qmake was not found at '$qmake'! Exiting..."
} else {
    Write-Debug "Qmake was found at '$qmake'."
}
if (!(Test-Path $varsall)) {
    Write-Output "vcvarsall not found at '$varsall'! Exiting..."
} else {
    Write-Debug "vcvarsall was found at '$varsall'."
}

$buildDir = "$PSScriptRoot\build"

if ($SkipCleaning -Or $SkipPreClean) {
    Write-Debug "Skipping pre cleaning"
    if (Test-Path $buildDir) {
        Remove-Item $buildDir -Force -Recurse
        Write-Debug "Build dir removed"
    }
} else {
    Invoke-Expression -Command "$PSScriptRoot\clean.ps1 $DeepCleanSwitch $VerboseSwitch"
}

# Create the build folder
if ($Verbose) {
    New-Item -Path $buildDir -ItemType "directory"
} else {
    $null = New-Item -Path $buildDir -ItemType "directory"
}
Write-Debug "Build dir created"

# Build the framework (this is done via bat because of vcvarsall)
$buildHelper = """$PSScriptRoot\build_helper.bat"" ""$varsall"" $WinSdk ""$qmake"" ""$jom"""
Write-Debug "Start creating the framework: $buildHelper"
if ($Verbose) {
    cmd.exe /c $buildHelper
} else {
    $null = cmd.exe /c $buildHelper
}
Write-Debug "Framework created"

# Copy the output to the build folder
Copy-Item "$PSScriptRoot\bin\binarycreator.*" $buildDir
Copy-Item "$PSScriptRoot\bin\installerbase.*" $buildDir
Copy-Item "$PSScriptRoot\tools\repocompare\release\repocompare.*" $buildDir

Write-Debug "Framework output to build"

if ($SkipCleaning -Or $SkipPostClean) {
    Write-Debug "Skipping post cleaning"
} else {
    Invoke-Expression -Command "$PSScriptRoot\clean.ps1 $DeepCleanSwitch $VerboseSwitch -SkipBuildDir"
}

Write-Output "---"
Write-Output "Framework built successfully. You'll find it under \build"

$DebugPreference = 'SilentlyContinue'
