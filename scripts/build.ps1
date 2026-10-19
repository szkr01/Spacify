<#
.SYNOPSIS
    Configures and builds the Spacify VST3 plug-in.

.DESCRIPTION
    Requires CMake >= 3.22 and a Visual Studio 2022 instance that has the
    "Desktop development with C++" workload installed.  Both the Community and
    the Build Tools editions work.

    JUCE must already be present at external/JUCE (see README.md).

.PARAMETER Configuration
    Debug or Release.  Defaults to Release.

.PARAMETER WithTools
    Also build the development-only helper executables (offline UI renderer and
    headless DSP tests).  These are never part of the shipped plug-in.

.PARAMETER Clean
    Delete the build directory before configuring.

.PARAMETER VsInstance
    Path to the Visual Studio instance to use.  Only needed when more than one
    VS 2022 instance is installed and CMake would pick the wrong one.

.EXAMPLE
    ./scripts/build.ps1
    ./scripts/build.ps1 -WithTools
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Release',

    [switch] $WithTools,

    [switch] $Clean,

    [string] $VsInstance = ''
)

$ErrorActionPreference = 'Stop'

$root     = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root 'build'

if (-not (Test-Path (Join-Path $root 'external/JUCE/CMakeLists.txt'))) {
    throw "JUCE was not found at external/JUCE. See the 'Building' section of README.md."
}

if ($Clean -and (Test-Path $buildDir)) {
    Remove-Item -Recurse -Force $buildDir
}

$configureArgs = @(
    '-S', $root,
    '-B', $buildDir,
    '-G', 'Visual Studio 17 2022',
    '-A', 'x64',
    "-DSPACIFY_BUILD_TOOLS=$(if ($WithTools) { 'ON' } else { 'OFF' })"
)

if ($VsInstance) {
    $configureArgs += "-DCMAKE_GENERATOR_INSTANCE=$VsInstance"
}

Write-Host ">> cmake $($configureArgs -join ' ')" -ForegroundColor Cyan
& cmake @configureArgs
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }

Write-Host ">> cmake --build $buildDir --config $Configuration" -ForegroundColor Cyan
& cmake --build $buildDir --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }

$vst3 = Join-Path $buildDir "Spacify_artefacts/$Configuration/VST3/Spacify.vst3"
$distDir = Join-Path $root 'dist'
$distVst3 = Join-Path $distDir 'Spacify.vst3'

if (Test-Path $vst3) {
    New-Item -ItemType Directory -Force -Path $distDir | Out-Null
    if (Test-Path $distVst3) {
        Remove-Item -Recurse -Force $distVst3
    }
    Copy-Item -Recurse -Force $vst3 $distVst3
}

Write-Host ''
Write-Host "Built: $vst3" -ForegroundColor Green
Write-Host "Synced: $distVst3" -ForegroundColor Green
Write-Host "Install by copying that folder into your VST3 directory." -ForegroundColor Green
