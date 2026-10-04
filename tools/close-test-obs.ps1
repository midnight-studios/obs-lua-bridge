#Requires -Version 7
<#
.SYNOPSIS
    Closes the test OBS the normal way (like clicking its close button) and
    waits for it to exit. Never kills it.

.DESCRIPTION
    If OBS doesn't exit within the timeout, lists its visible windows
    (tools/obs-windows.ps1) so the blocker can be identified, and exits with
    code 1. Exits 0 when OBS closed (or wasn't running).
#>
param(
    [string] $ObsPath = 'C:\obs-test\bin\64bit\obs64.exe',
    [int] $TimeoutSeconds = 30
)

$process = Get-Process obs64 -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $ObsPath } |
    Select-Object -First 1
if (-not $process) {
    Write-Host 'test OBS is not running'
    exit 0
}

$started = Get-Date
[void]$process.CloseMainWindow()
if ($process.WaitForExit($TimeoutSeconds * 1000)) {
    Write-Host ("test OBS closed in {0:N1} s" -f ((Get-Date) - $started).TotalSeconds)
    exit 0
}

Write-Host "test OBS did not close within $TimeoutSeconds s; not forcing it. Its windows:"
& "$PSScriptRoot/obs-windows.ps1" -ObsPath $ObsPath
exit 1
