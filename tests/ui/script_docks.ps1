#Requires -Version 7
<#
.SYNOPSIS
    In-OBS test of per-script docks (Windows): drives the real OBS UI through UI Automation.

.DESCRIPTION
    Runs against a portable test OBS with the plugin deployed (tools/deploy-test.ps1), the
    obs-websocket server enabled and the collections from tests/websocket/setup_test_obs.py
    ("LuaBridge Docks", "LuaBridge B", "LuaBridge Empty") and the profile "LuaBridge P2".

    It starts and closes OBS several times. It never kills OBS: if OBS doesn't close, the test
    stops and says so. Input goes to OBS through UI Automation and posted window messages, so
    the real mouse and keyboard are never used; a new dock does take focus when it opens.

    Phases:
      1. Tools > Lua Bridge lists the owners; ticking Dock adds a floating dock and saves
         docks.json; a custom title renames the dock and its Docks menu entry; a dock is
         moved, another hidden.
      2. After a restart, both docks are back with the same geometry, the hidden one hidden.
         A collection without the script shows "isn't running"; profile switches keep the
         docks; switching back shows the controls. Unticking removes a dock. OBS closes with
         a confirm dialog open in a script dock (dock-test's Reset).
      3. Starting in a collection without the script: the dock waits; the window says
         "Not loaded"; switching collections brings the controls.
         A dock re-added in the same session opens floating; clearing a title restores the
         display name.
      4. A corrupt docks.json is renamed to docks.json.invalid and OBS starts without
         script docks; with no scripts, the window says so.
      5. When the only row is a dock whose script isn't loaded, unticking it empties the table.
    Every OBS run must end with 0 memory leaks and no unexpected Lua Bridge warnings.

    The test deletes docks.json (and docks.json.invalid) in the test OBS before it starts
    and when it finishes.

.EXAMPLE
    pwsh tests/ui/script_docks.ps1 -Root C:\obs-test
#>
param(
    [string] $Root = 'C:\obs-test',
    [switch] $Legacy33 # OBS 33 test copy: the plugin is in <Root>\_legacy (see TESTING.md)
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$exe = Join-Path $Root 'bin\64bit\obs64.exe'
$logDir = Join-Path $Root 'config\obs-studio\logs'
$configDir = Join-Path $Root 'config\obs-studio\plugin_config\obs-lua-bridge'
$docksJson = Join-Path $configDir 'docks.json'
$collection = 'LuaBridge Docks'
$otherCollection = 'LuaBridge B'
$emptyCollection = 'LuaBridge Empty'

Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes
$AE = [System.Windows.Automation.AutomationElement]
$Scope = [System.Windows.Automation.TreeScope]
$True_ = [System.Windows.Automation.Condition]::TrueCondition

if (-not ('ScriptDocksTest.Win' -as [type])) {
    Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
namespace ScriptDocksTest {
public static class Win {
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X; public int Y; }
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool ScreenToClient(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
    static IntPtr Lp(int x, int y) { return (IntPtr)((y << 16) | (x & 0xFFFF)); }
    public static void Click(IntPtr hwnd, int sx, int sy) {
        POINT p; p.X = sx; p.Y = sy; ScreenToClient(hwnd, ref p);
        PostMessage(hwnd, 0x0200, IntPtr.Zero, Lp(p.X, p.Y));
        PostMessage(hwnd, 0x0201, (IntPtr)1, Lp(p.X, p.Y));
        PostMessage(hwnd, 0x0202, IntPtr.Zero, Lp(p.X, p.Y));
    }
    public static void Key(IntPtr hwnd, int vk) {
        PostMessage(hwnd, 0x0100, (IntPtr)vk, (IntPtr)1);
        PostMessage(hwnd, 0x0101, (IntPtr)vk, (IntPtr)unchecked((int)0xC0000001));
    }
}
}
'@
}

# --- Results -------------------------------------------------------------------------------

$script:checks = 0
$script:failures = [System.Collections.Generic.List[string]]::new()
function Check([bool] $ok, [string] $what) {
    $script:checks++
    if ($ok) { Write-Host "  ok    $what" } else { Write-Host "  FAIL  $what" -ForegroundColor Red; $script:failures.Add($what) }
}

# --- OBS process ---------------------------------------------------------------------------

$script:log = $null
function Start-Obs([string] $Collection) {
    if (Get-Process obs64 -ErrorAction SilentlyContinue) { throw 'an obs64 process is already running; close it first' }
    if ($Legacy33) {
        $env:OBS_LEGACY_PLUGINS_PATH = Join-Path $Root '_legacy\obs-lua-bridge\bin\64bit'
        $env:OBS_LEGACY_PLUGINS_DATA_PATH = Join-Path $Root '_legacy\data-root'
    }
    $before = if (Test-Path $logDir) { @(Get-ChildItem $logDir -Filter *.txt | ForEach-Object Name) } else { @() }
    $null = Start-Process $exe -ArgumentList '--collection', "`"$Collection`"" -WorkingDirectory (Split-Path $exe) -PassThru
    $deadline = (Get-Date).AddSeconds(90)
    $script:log = $null
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 500
        if (-not $script:log -and (Test-Path $logDir)) {
            $script:log = Get-ChildItem $logDir -Filter *.txt | Where-Object { $_.Name -notin $before } |
                Sort-Object LastWriteTime | Select-Object -Last 1
        }
        if ($script:log -and (Select-String -Path $script:log.FullName -SimpleMatch '==== Startup complete' -Quiet)) {
            Start-Sleep -Seconds 3 # scripts register right after startup
            return
        }
    }
    throw 'OBS did not finish starting within 90 s'
}

function Stop-Obs {
    & "$repo\tools\close-test-obs.ps1" -ObsPath $exe -TimeoutSeconds 60 | Out-Host
    $p = Get-Process obs64 -ErrorAction SilentlyContinue
    if ($p -and -not $p.WaitForExit(90000)) { throw 'OBS is still running after the close request; not killing it' }
    $text = Get-Content $script:log.FullName -Raw
    $leaks = [regex]::Match($text, 'Number of memory leaks: (\d+)')
    Check ($leaks.Success -and $leaks.Groups[1].Value -eq '0') "0 memory leaks ($($script:log.Name))"
}

function Get-LogLines([string] $Pattern) {
    @(Select-String -Path $script:log.FullName -Pattern $Pattern | ForEach-Object Line)
}

function Assert-NoUnexpectedWarnings([string[]] $Allowed = @()) {
    $warnings = Get-LogLines '\[lua-bridge\].*(could not|invalid|error|warning)' |
        Where-Object { $line = $_; -not ($Allowed | Where-Object { $line -match $_ }) }
    Check ($warnings.Count -eq 0) "no unexpected Lua Bridge warnings $(if ($warnings) { ': ' + ($warnings -join ' | ') })"
}

function Obs-Ctl([string[]] $Arguments) {
    $out = & py -I "$PSScriptRoot\obs_ctl.py" @Arguments
    if ($LASTEXITCODE -ne 0) { throw "obs_ctl.py $Arguments failed: $out" }
    $out
}

function Switch-Collection([string] $Name) {
    Obs-Ctl collection, $Name | Out-Null
    Start-Sleep -Seconds 4 # scripts unload and load
}

# --- UI Automation -------------------------------------------------------------------------

function Get-MainWindow {
    $obs = Get-Process obs64 | Select-Object -First 1
    $cond = [System.Windows.Automation.PropertyCondition]::new($AE::ProcessIdProperty, $obs.Id)
    $AE::RootElement.FindAll($Scope::Children, $cond) | Where-Object { $_.Current.Name -match '^OBS \d' } | Select-Object -First 1
}

function Find-All($root, $property, $value) {
    @($root.FindAll($Scope::Descendants, [System.Windows.Automation.PropertyCondition]::new($property, $value)))
}

function Get-Dock([string] $Title) {
    Find-All (Get-MainWindow) $AE::ClassNameProperty 'OBSDock' | Where-Object { $_.Current.Name -eq $Title } | Select-Object -First 1
}

function Get-Rect($el) { $r = $el.Current.BoundingRectangle; '{0},{1},{2},{3}' -f [int]$r.X, [int]$r.Y, [int]$r.Width, [int]$r.Height }

function Same-Rect([string] $a, [string] $b) {
    $x = $a -split ','; $y = $b -split ','
    for ($i = 0; $i -lt 4; $i++) { if ([math]::Abs([int]$x[$i] - [int]$y[$i]) -gt 2) { return $false } }
    return $true
}

function Get-Texts($root) { Find-All $root $AE::ControlTypeProperty ([System.Windows.Automation.ControlType]::Text) | ForEach-Object { $_.Current.Name } }
function Get-VisibleTexts($root) {
    Find-All $root $AE::ControlTypeProperty ([System.Windows.Automation.ControlType]::Text) |
        Where-Object { -not $_.Current.IsOffscreen } | ForEach-Object { $_.Current.Name }
}
function Get-Buttons($root) { Find-All $root $AE::ControlTypeProperty ([System.Windows.Automation.ControlType]::Button) | ForEach-Object { $_.Current.Name } }

function Get-MenuEntries([string] $Menu) {
    $item = Find-All (Get-MainWindow) $AE::NameProperty $Menu | Where-Object { $_.Current.ControlType.ProgrammaticName -eq 'ControlType.MenuItem' } | Select-Object -First 1
    $expand = $item.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern)
    # Close any other menu of the menu bar first, then open this one; Qt sometimes
    # ignores an Expand (while another popup closes, or right after a collection
    # switch), so try again until the menu reports Expanded with entries
    foreach ($other in Find-All (Get-MainWindow) $AE::ClassNameProperty 'QAction') {
        $p = $null
        if ($other.TryGetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern, [ref] $p) -and
            $p.Current.ExpandCollapseState -eq 'Expanded') { $p.Collapse() }
    }
    $deadline = (Get-Date).AddSeconds(4)
    do {
        $expand.Expand()
        Start-Sleep -Milliseconds 600
        $open = $expand.Current.ExpandCollapseState -eq 'Expanded' -and
            @($item.FindAll($Scope::Descendants, $True_)).Count -gt 1
    } while (-not $open -and (Get-Date) -lt $deadline)
    $entries = @{}
    foreach ($e in $item.FindAll($Scope::Descendants, $True_)) {
        if ($e.Current.ControlType.ProgrammaticName -ne 'ControlType.MenuItem') { continue }
        $toggle = $null
        $state = if ($e.TryGetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern, [ref] $toggle)) { "$($toggle.Current.ToggleState)" } else { '' }
        $entries[$e.Current.Name] = @{ Element = $e; State = $state }
    }
    return @{ Entries = $entries; Collapse = { $expand.Collapse() }.GetNewClosure() }
}

function Open-DocksWindow {
    # A menu sometimes isn't populated yet when it is read; try again for a few seconds
    $deadline = (Get-Date).AddSeconds(5)
    do {
        $menu = Get-MenuEntries 'Tools'
        $entry = $menu.Entries['Lua Bridge']
        if (-not $entry) { & $menu.Collapse; Start-Sleep -Milliseconds 500 }
    } while (-not $entry -and (Get-Date) -lt $deadline)
    if (-not $entry) {
        & "$repo\tools\obs-windows.ps1" -ObsPath $exe | Out-Host
        throw "Tools > Lua Bridge not found; the Tools menu had: $($menu.Entries.Keys -join ', ')"
    }
    $entry.Element.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
    Start-Sleep -Milliseconds 800
    Get-DocksWindow
}

function Get-DocksWindow { Find-All (Get-MainWindow) $AE::NameProperty 'Lua Bridge Docks' | Select-Object -First 1 }

# Rows of the docks window: owner -> @{ Script; Status; Dock (element); DockState; Title (element) }
function Get-DocksRows {
    $window = Get-DocksWindow
    $table = $window.FindFirst($Scope::Descendants, [System.Windows.Automation.PropertyCondition]::new($AE::ClassNameProperty, 'QTableWidget'))
    $rows = [ordered]@{}
    if (-not $table -or $table.Current.IsOffscreen) { return $rows } # the "no scripts" page is shown
    $cells = @($table.FindAll($Scope::Children, $True_) | Where-Object { $_.Current.ControlType.ProgrammaticName -eq 'ControlType.DataItem' })
    for ($i = 0; $i + 4 -lt $cells.Count + 0; $i += 5) {
        $dock = $cells[$i + 3]
        $rows[$cells[$i + 1].Current.Name] = @{
            Script = $cells[$i].Current.Name; Owner = $cells[$i + 1]; Status = $cells[$i + 2].Current.Name; Dock = $dock
            DockState = "$($dock.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern).Current.ToggleState)"
            Title = $cells[$i + 4]
        }
    }
    $rows
}

# Ticks or unticks Dock the way a keyboard user does: select the row's Owner ID cell, Right
# twice, Space. Keys go to Qt's focus window, so the docks window is clicked first.
function Toggle-Dock([string] $Owner) {
    $window = Get-DocksWindow
    $hwnd = [IntPtr]$window.Current.NativeWindowHandle
    $row = (Get-DocksRows)[$Owner]
    $r = $row.Owner.Current.BoundingRectangle
    [ScriptDocksTest.Win]::Click($hwnd, [int]($r.X + $r.Width / 2), [int]($r.Y + $r.Height / 2))
    Start-Sleep -Milliseconds 300
    foreach ($vk in 0x27, 0x27, 0x20) { [ScriptDocksTest.Win]::Key($hwnd, $vk); Start-Sleep -Milliseconds 150 }
    Start-Sleep -Milliseconds 1200
}

function Set-DockTitle([string] $Owner, [string] $Title) {
    $row = (Get-DocksRows)[$Owner]
    $row.Title.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern).SetValue($Title)
    Start-Sleep -Milliseconds 800
}

function Close-Window($el) {
    $null = [ScriptDocksTest.Win]::PostMessage([IntPtr]$el.Current.NativeWindowHandle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 800
}

function Read-DocksJson { if (Test-Path $docksJson) { Get-Content $docksJson -Raw | ConvertFrom-Json } }

# --- Test ----------------------------------------------------------------------------------

if (Get-Process obs64 -ErrorAction SilentlyContinue) { throw 'an obs64 process is already running; close it first' }
Remove-Item $docksJson, "$docksJson.invalid", "$docksJson.bak" -ErrorAction SilentlyContinue

# Whatever happens, close OBS normally (never kill it) and remove the test's files
try {
Write-Host "`n== 1. Add docks from Tools > Lua Bridge"
Start-Obs $collection
$window = Open-DocksWindow
Check ($null -ne $window) 'Tools > Lua Bridge opens the Lua Bridge Docks window'
$rows = Get-DocksRows
Check ((($rows.Keys | Sort-Object) -join ',') -eq 'docktest,ping,pong,stopwatch,wstest') "window lists the owners ($($rows.Keys -join ','))"
Check ($rows['stopwatch'].Status -eq 'Running' -and $rows['stopwatch'].DockState -eq 'Off') 'stopwatch: Running, no dock'

Toggle-Dock 'stopwatch'
$dock = Get-Dock 'Stopwatch (demo)'
Check ($null -ne $dock -and -not $dock.Current.IsOffscreen) 'ticking Dock opens the dock "Stopwatch (demo)"'
Check ([IntPtr]$dock.Current.NativeWindowHandle -ne [IntPtr]::Zero) 'the new dock is floating'
Check ((Get-Buttons $dock) -contains 'Reset') 'the dock shows the stopwatch controls'
Check ((Get-LogLines "added a script dock for 'stopwatch'").Count -eq 1) 'log: added a script dock'
Toggle-Dock 'ping'
Check ($null -ne (Get-Dock 'Ping')) 'second dock "Ping" added'
$saved = Read-DocksJson
Check ((($saved.docks | ForEach-Object owner) -join ',') -eq 'stopwatch,ping') 'docks.json lists stopwatch, ping in order'

Set-DockTitle 'stopwatch' '  Big Timer '
$saved = Read-DocksJson
Check (($saved.docks | Where-Object owner -eq 'stopwatch').title -eq 'Big Timer') 'custom title saved, trimmed'
$dock = Get-Dock 'Big Timer'
Check ($null -ne $dock) 'the dock is renamed "Big Timer"'
$menu = Get-MenuEntries 'Docks'; & $menu.Collapse
Check ($menu.Entries.ContainsKey('Big Timer') -and -not $menu.Entries.ContainsKey('Stopwatch (demo)')) 'Docks menu shows "Big Timer"'

# Move and resize the floating dock, hide Ping
# (SetWindowPos sizes the outer frame; UI Automation reports Qt's geometry, a few pixels
# different, so the geometry after the move is what must come back)
$before = Get-Rect $dock
[void][ScriptDocksTest.Win]::SetWindowPos([IntPtr]$dock.Current.NativeWindowHandle, [IntPtr]::Zero, 220, 180, 520, 600, 0x0014)
Start-Sleep -Milliseconds 800
$bigRect = Get-Rect (Get-Dock 'Big Timer')
Check (-not (Same-Rect $bigRect $before)) "dock moved and resized ($before -> $bigRect)"
# Hide Ping from the Docks menu (closing a dock with its X makes OBS show a modal notice)
$menu = Get-MenuEntries 'Docks'
$menu.Entries['Ping'].Element.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
# The menu can take a moment to reflect the change
$deadline = (Get-Date).AddSeconds(3)
do {
    Start-Sleep -Milliseconds 500
    $menu = Get-MenuEntries 'Docks'; & $menu.Collapse
} while ($menu.Entries['Ping'].State -ne 'Off' -and (Get-Date) -lt $deadline)
Check ($menu.Entries['Ping'].State -eq 'Off') 'Ping dock hidden (Docks menu unticked)'
Close-Window (Get-DocksWindow)
Assert-NoUnexpectedWarnings
Stop-Obs

Write-Host "`n== 2. Restart, collection and profile switches, removal, exit with a dialog open"
Start-Obs $collection
Check ((Get-LogLines 'created 2 script dock\(s\)').Count -eq 1) 'log: created 2 script docks at load'
$dock = Get-Dock 'Big Timer'
Check ($null -ne $dock -and -not $dock.Current.IsOffscreen) '"Big Timer" is back and visible'
Check ($dock -and (Same-Rect (Get-Rect $dock) $bigRect)) "same position and size ($(if ($dock) { Get-Rect $dock }))"
$menu = Get-MenuEntries 'Docks'; & $menu.Collapse
Check ($menu.Entries['Ping'].State -eq 'Off' -and $menu.Entries['Big Timer'].State -eq 'On') 'Ping still hidden, Big Timer shown'

Switch-Collection $otherCollection
$dock = Get-Dock 'Big Timer'
Check (@((Get-Texts $dock) -match "isn't running").Count -gt 0) 'other collection: the dock says the script isn''t running'
Obs-Ctl profile, 'LuaBridge P2' | Out-Null; Start-Sleep -Seconds 3
$dock = Get-Dock 'Big Timer'
Check ($dock -and (Same-Rect (Get-Rect $dock) $bigRect)) 'profile switch: dock unchanged'
Obs-Ctl profile, 'Untitled' | Out-Null; Start-Sleep -Seconds 3
Switch-Collection $collection
$dock = Get-Dock 'Big Timer'
Check ((Get-Buttons $dock) -contains 'Reset') 'back in the collection: the controls are back'
Check ($dock -and (Same-Rect (Get-Rect $dock) $bigRect)) 'collection switches: dock unchanged'

$null = Open-DocksWindow
Toggle-Dock 'ping'
$menu = Get-MenuEntries 'Docks'; & $menu.Collapse
Check (-not $menu.Entries.ContainsKey('Ping')) 'unticking removes the Ping dock from the Docks menu'
Check ((((Read-DocksJson).docks | ForEach-Object owner) -join ',') -eq 'stopwatch') 'docks.json lists only stopwatch'
# Re-adding in the same session: OBS no longer has a layout for it, so it opens floating again
Toggle-Dock 'ping'
$dock = Get-Dock 'Ping'
Check ($dock -and -not $dock.Current.IsOffscreen -and [IntPtr]$dock.Current.NativeWindowHandle -ne [IntPtr]::Zero) 're-adding Ping in the same session opens it floating'
Toggle-Dock 'ping'
Check ($null -eq (Get-Dock 'Ping')) 'and unticking removes it again'
Close-Window (Get-DocksWindow)

# A confirm dialog open in a script dock must not block or crash the exit, nor run its command
# (dock-test logs every command it receives)
$null = Open-DocksWindow
Toggle-Dock 'docktest'
Close-Window (Get-DocksWindow)
$start = Find-All (Get-Dock 'Dock Test') $AE::NameProperty 'Start' | Select-Object -First 1
$start.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
Start-Sleep -Milliseconds 800
Check ((Get-LogLines 'dock-test\.lua\] command start origin=dock').Count -eq 1) 'a button in a script dock sends its command'
$reset = Find-All (Get-Dock 'Dock Test') $AE::NameProperty 'Reset' | Select-Object -First 1
$reset.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
Start-Sleep -Milliseconds 800
$confirm = @(Find-All (Get-MainWindow) $AE::ClassNameProperty 'QMessageBox')
Check ($confirm.Count -eq 1 -and @((Get-Texts $confirm[0]) -match "Run 'Reset'\?").Count -eq 1) 'Reset opens its confirm dialog'
Assert-NoUnexpectedWarnings
Stop-Obs
Check ((Get-LogLines 'dock-test\.lua\] command reset').Count -eq 0) 'the confirm was not run at exit'

Write-Host "`n== 3. Script loads late: the dock waits"
Start-Obs $otherCollection
$dock = Get-Dock 'Big Timer'
Check ($null -ne $dock) 'dock created although its script is not loaded'
Check (@((Get-Texts $dock) -match 'Waiting for the script').Count -gt 0) 'the dock says it is waiting for the script'
$null = Open-DocksWindow
$rows = Get-DocksRows
Check ($rows['stopwatch'].Status -eq 'Not loaded' -and $rows['stopwatch'].DockState -eq 'On') 'window: stopwatch Not loaded, Dock ticked'
Check ($rows['stopwatch'].Script -eq 'Stopwatch (demo)') 'window: the saved display name is shown'
Close-Window (Get-DocksWindow)
Switch-Collection $collection
Check ((Get-Buttons (Get-Dock 'Big Timer')) -contains 'Reset') 'the controls appear when the script registers'
$null = Open-DocksWindow
Set-DockTitle 'stopwatch' ''
Close-Window (Get-DocksWindow)
Check ($null -ne (Get-Dock 'Stopwatch (demo)')) 'clearing the title gives the dock the display name again'
Check ($null -eq ((Read-DocksJson).docks | Where-Object owner -eq 'stopwatch').title) 'docks.json has no title for it'
Assert-NoUnexpectedWarnings
Stop-Obs

Write-Host "`n== 4. Corrupt docks.json, no scripts"
Set-Content $docksJson '{broken' -NoNewline
Start-Obs $emptyCollection
Check ((Get-LogLines 'docks.json: not a JSON object; starting without script docks').Count -eq 1) 'log: the file is invalid'
Check (Test-Path "$docksJson.invalid") 'the file was renamed to docks.json.invalid'
Check ($null -eq (Get-Dock 'Stopwatch (demo)')) 'no script docks'
$null = Open-DocksWindow
Check (@(Get-VisibleTexts (Get-DocksWindow) | Where-Object { $_ -match 'No scripts are using Lua Bridge yet' }).Count -eq 1) 'the window says no scripts are registered'
Close-Window (Get-DocksWindow)
Assert-NoUnexpectedWarnings @('docks\.json: not a JSON object')
Stop-Obs

Write-Host "`n== 5. The only row: a dock whose script isn't loaded"
Remove-Item "$docksJson.invalid" -ErrorAction SilentlyContinue
Set-Content $docksJson '{"version":1,"docks":[{"owner":"gone","display_name":"Gone Script"}]}'
Start-Obs $emptyCollection
$null = Open-DocksWindow
$rows = Get-DocksRows
Check ((($rows.Keys) -join ',') -eq 'gone' -and $rows['gone'].Status -eq 'Not loaded') 'window: one row, Gone Script, Not loaded'
Toggle-Dock 'gone'
Check ((Get-DocksRows).Count -eq 0) 'unticking the only row empties the table'
Check (@(Get-VisibleTexts (Get-DocksWindow) | Where-Object { $_ -match 'No scripts are using Lua Bridge yet' }).Count -eq 1) 'and the window says no scripts are registered'
Check ($null -eq (Get-Dock 'Gone Script')) 'the dock is gone'
Close-Window (Get-DocksWindow)
Assert-NoUnexpectedWarnings
Stop-Obs
} finally {
    if (Get-Process obs64 -ErrorAction SilentlyContinue) {
        Write-Host 'test stopped early; closing OBS'
        try { Stop-Obs } catch { Write-Host $_ }
    }
    Remove-Item $docksJson, "$docksJson.invalid", "$docksJson.bak" -ErrorAction SilentlyContinue
}
Write-Host ''
if ($script:failures.Count) {
    Write-Host "script_docks: $($script:checks) checks, $($script:failures.Count) FAILED" -ForegroundColor Red
    $script:failures | ForEach-Object { Write-Host "  - $_" }
    exit 1
}
Write-Host "script_docks: $($script:checks) checks, all passed" -ForegroundColor Green
