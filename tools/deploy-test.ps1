#Requires -Version 7
<#
.SYNOPSIS
    Deploys the built plugin into a portable test OBS at C:/obs-test.

.DESCRIPTION
    On first run, copies the installed OBS into the test directory and enables
    portable mode so its config and logs live in <TestRoot>/config instead of
    %APPDATA%. Then copies the plugin from build_x64/rundir/<Configuration> using
    the classic portable layout:
      <TestRoot>/obs-plugins/64bit/<name>.dll (+ .pdb)
      <TestRoot>/data/obs-plugins/<name>/
#>
param(
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo',
    [string] $TestRoot = 'C:/obs-test',
    [string] $ObsInstall = 'C:/Program Files/obs-studio',
    [switch] $RefreshObs
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent
$pluginName = (Get-Content "$repoRoot/buildspec.json" -Raw | ConvertFrom-Json).name

function Copy-Tree([string] $Source, [string] $Destination) {
    robocopy $Source $Destination /E /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE): $Source -> $Destination" }
    $global:LASTEXITCODE = 0
}

# Create the test OBS instance if needed
if ($RefreshObs -or -not (Test-Path "$TestRoot/bin/64bit/obs64.exe")) {
    if (-not (Test-Path "$ObsInstall/bin/64bit/obs64.exe")) {
        throw "OBS not found at $ObsInstall"
    }
    Write-Host "Copying OBS from $ObsInstall to $TestRoot ..."
    foreach ($dir in 'bin', 'data', 'obs-plugins') {
        Copy-Tree "$ObsInstall/$dir" "$TestRoot/$dir"
    }
}
New-Item -ItemType File -Force "$TestRoot/portable_mode.txt" | Out-Null

# Built output for the requested configuration
$configDir = "$repoRoot/build_x64/rundir/$Configuration"
$dll = Get-Item "$configDir/$pluginName.dll" -ErrorAction SilentlyContinue
if (-not $dll) { throw "No $Configuration build of $pluginName.dll found in $configDir" }
Write-Host "Deploying $pluginName ($Configuration) from $configDir"

# Binaries
$binDest = "$TestRoot/obs-plugins/64bit"
New-Item -ItemType Directory -Force $binDest | Out-Null
Copy-Item $dll.FullName $binDest -Force
$pdb = Join-Path $configDir "$pluginName.pdb"
if (Test-Path $pdb) { Copy-Item $pdb $binDest -Force }

# Data (locale etc.)
$dataSrc = Join-Path $configDir $pluginName
if (Test-Path $dataSrc) {
    Copy-Tree $dataSrc "$TestRoot/data/obs-plugins/$pluginName"
}

Write-Host "Deployed to $TestRoot"
