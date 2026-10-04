#Requires -Version 7
<#
.SYNOPSIS
    Lists the visible top-level windows of a running OBS (titles, classes,
    enabled state), to see what blocks OBS from closing.

.DESCRIPTION
    A main window that is disabled usually means a modal dialog is open; that
    dialog is then listed as another window of the same process.
#>
param(
    [string] $ObsPath = 'C:\obs-test\bin\64bit\obs64.exe'
)

$ErrorActionPreference = 'Stop'

if (-not ('ObsWindows' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class ObsWindows {
    delegate bool EnumProc(IntPtr hwnd, IntPtr lParam);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc cb, IntPtr lParam);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] static extern bool IsWindowEnabled(IntPtr hwnd);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowText(IntPtr hwnd, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassName(IntPtr hwnd, StringBuilder s, int n);

    public static List<string[]> List(uint processId) {
        var result = new List<string[]>();
        EnumWindows((hwnd, _) => {
            uint pid;
            GetWindowThreadProcessId(hwnd, out pid);
            if (pid == processId && IsWindowVisible(hwnd)) {
                var title = new StringBuilder(512);
                var cls = new StringBuilder(256);
                GetWindowText(hwnd, title, title.Capacity);
                GetClassName(hwnd, cls, cls.Capacity);
                result.Add(new[] { title.ToString(), cls.ToString(), IsWindowEnabled(hwnd) ? "enabled" : "DISABLED" });
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }
}
'@
}

$processes = Get-Process obs64 -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $ObsPath }
if (-not $processes) {
    Write-Host "OBS ($ObsPath) is not running"
    return
}
foreach ($process in $processes) {
    Write-Host "OBS pid $($process.Id) visible top-level windows:"
    foreach ($w in [ObsWindows]::List([uint32]$process.Id)) {
        $title = if ($w[0]) { $w[0] } else { '(no title)' }
        Write-Host ("  {0,-9} {1,-40} {2}" -f $w[2], $w[1], $title)
    }
}
