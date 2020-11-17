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

$DeepCleanSwitch = ""
if ($DeepClean) { $DeepCleanSwitch = "-DeepClean" }
$VerboseSwitch = ""
if ($Verbose) { $VerboseSwitch = "-Verbose" }

$jom = "$ToolsPrefix\jom\jom.exe"
$qmake = "$ToolsPrefix\Qt_static\v5.12.7\bin\qmake.exe"
$varsall = "C:\$BuildToolsPrefix\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"

Write-Output "Start building installer framework"
Write-Output "---"

if (!(Test-Path $jom)) {
    Write-Output "Jom was not found at '$jom'! Exiting..."
    Return
} else {
    if ($Verbose) { Write-Output "Jom was found at '$jom'." }
}
if (!(Test-Path $qmake)) {
    Write-Output "Qmake was not found at '$qmake'! Exiting..."
} else {
    if ($Verbose) { Write-Output "Qmake was found at '$qmake'." }
}
if (!(Test-Path $varsall)) {
    Write-Output "vcvarsall not found at '$varsall'! Exiting..."
} else {
    if ($Verbose) { Write-Output "vcvarsall was found at '$varsall'." }
}

$buildDir = "$PSScriptRoot\build"

if ($SkipCleaning -Or $SkipPreClean) {
    if ($Verbose) { Write-Output "Skipping pre cleaning" }
    if (Test-Path $buildDir) {
        Remove-Item $buildDir -Force -Recurse
        if ($Verbose) { Write-Output "Build dir removed" }
    }
} else {
    Invoke-Expression -Command "$PSScriptRoot\clean.ps1 $DeepCleanSwitch $VerboseSwitch"
}

# Create the build folder
if ($Verbose) {
    New-Item -Path $buildDir -ItemType "directory"
    Write-Output "Build dir created"
} else {
    $null = New-Item -Path $buildDir -ItemType "directory"
}

# # Invokes a Cmd.exe shell script and updates the environment.
# function Invoke-CmdScript {
#   param(
#     [String] $ScriptName
#   )
#   $cmdLine = """$ScriptName"" $args & set"
#   & $Env:SystemRoot\system32\cmd.exe /c $cmdLine |
#   Select-String '^([^=]*)=(.*)$' | Foreach-Object {
#     $varName = $_.Matches[0].Groups[1].Value
#     $varValue = $_.Matches[0].Groups[2].Value
#     Set-Item Env:$varName $varValue
#   }
# }

# $location = Get-Location

# We need to run this from this folder
# Set-Location $PSScriptRoot

# # Store the environment vars from vcvarsall.bat
# Invoke-CmdScript "$varsall" "x86" "$WinSdk"

# # Create the makefiles
# & "$qmake" "-r"

# # Build the makefiels
# & "$jom" "release"

# $params = """$varsall"" $WinSdk ""$qmake"" ""$jom"""
# Write-Output $params
$buildHelper = """$PSScriptRoot\build_helper.bat"" ""$varsall"" $WinSdk ""$qmake"" ""$jom"""

if ($Verbose) {
    Write-Output "Start creating the framework: " $buildHelper
    cmd.exe /c $buildHelper
    Write-Output "Framework created"
} else {
    $null = cmd.exe /c $buildHelper
}
# Set-Location $location

# Copy the output to the build folder
Copy-Item "$PSScriptRoot\bin\binarycreator.exe" $buildDir
Copy-Item "$PSScriptRoot\bin\installerbase.exe" $buildDir

if ($Verbose) { Write-Output "Framework output to build" }

if ($SkipCleaning -Or $SkipPostClean) {
    if ($Verbose) { Write-Output "Skipping post cleaning" }
} else {
    Invoke-Expression -Command "$PSScriptRoot\clean.ps1 $DeepCleanSwitch $VerboseSwitch -SkipBuildDir"
}

Write-Output "---"
Write-Output "Framework built successfully. You'll find it under \build"
