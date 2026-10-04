#Requires -Version 7
<#
.SYNOPSIS
    Closes the test OBS the normal way and waits for it to exit. Never kills it.

.DESCRIPTION
    Finds the OBS main window itself (title "OBS <version> ..."), not
    Process.MainWindowHandle: whenever a Lua script logs a warning, OBS shows
    its "Script Log" window, and .NET then reports that window as the main
    window, so CloseMainWindow() would only close the Script Log.

    -Method SysCommand (default) posts WM_SYSCOMMAND/SC_CLOSE to the main
    window: exactly what clicking its close button or pressing Alt+F4 does.
    -Method WmClose posts a bare WM_CLOSE to it.

    If OBS doesn't exit within the timeout, lists its visible windows
    (tools/obs-windows.ps1) so the blocker can be identified, and exits with
    code 1. Exits 0 when OBS closed (or wasn't running).
#>
param(
    [string] $ObsPath = 'C:\obs-test\bin\64bit\obs64.exe',
    [int] $TimeoutSeconds = 30,
    [ValidateSet('SysCommand', 'WmClose')]
    [string] $Method = 'SysCommand'
)

if (-not ('CloseTestObs' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class CloseTestObs {
    delegate bool EnumProc(IntPtr hwnd, IntPtr lParam);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc cb, IntPtr lParam);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] static extern IntPtr GetWindow(IntPtr hwnd, uint cmd);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowText(IntPtr hwnd, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint msg, IntPtr w, IntPtr l);

    // The visible, unowned top-level window titled "OBS <version> ..."
    public static IntPtr MainWindow(uint processId) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((hwnd, _) => {
            uint pid;
            GetWindowThreadProcessId(hwnd, out pid);
            if (pid != processId || !IsWindowVisible(hwnd) || GetWindow(hwnd, 4 /* GW_OWNER */) != IntPtr.Zero)
                return true;
            var title = new StringBuilder(512);
            GetWindowText(hwnd, title, title.Capacity);
            if (title.ToString().StartsWith("OBS ")) {
                found = hwnd;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
'@
}

$process = Get-Process obs64 -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $ObsPath } |
    Select-Object -First 1
if (-not $process) {
    Write-Host 'test OBS is not running'
    exit 0
}

$hwnd = [CloseTestObs]::MainWindow([uint32]$process.Id)
if ($hwnd -eq [IntPtr]::Zero) {
    Write-Host 'could not find the OBS main window; not forcing it. Its windows:'
    & "$PSScriptRoot/obs-windows.ps1" -ObsPath $ObsPath
    exit 1
}

$started = Get-Date
if ($Method -eq 'SysCommand') {
    # WM_SYSCOMMAND (0x0112) with SC_CLOSE (0xF060): the window's close button
    [void][CloseTestObs]::PostMessage($hwnd, 0x0112, [IntPtr]0xF060, [IntPtr]::Zero)
} else {
    # WM_CLOSE (0x0010)
    [void][CloseTestObs]::PostMessage($hwnd, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
}
if ($process.WaitForExit($TimeoutSeconds * 1000)) {
    Write-Host ("test OBS closed in {0:N1} s ({1})" -f ((Get-Date) - $started).TotalSeconds, $Method)
    exit 0
}

Write-Host "test OBS did not close within $TimeoutSeconds s ($Method, hwnd $hwnd); not forcing it. Its windows:"
& "$PSScriptRoot/obs-windows.ps1" -ObsPath $ObsPath
exit 1
