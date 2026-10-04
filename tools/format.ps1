#Requires -Version 7
<#
.SYNOPSIS
    Formats C/C++ sources with clang-format 19 and CMake files with gersemi.

.DESCRIPTION
    Mirrors build-aux/run-clang-format and build-aux/run-gersemi, which CI uses.
    clang-format runs on src/ and tests/ (vendored src/third-party/ is skipped);
    gersemi runs on CMakeLists.txt and cmake/. Use -Check to report files that
    need formatting without changing them.
#>
param(
    [switch] $Check
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path $PSScriptRoot -Parent

function Find-Tool([string[]] $Names) {
    foreach ($name in $Names) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
    }
    throw "None of $($Names -join ', ') found on PATH"
}

# clang-format must be 19.x to match CI
$clangFormat = Find-Tool 'clang-format-19', 'clang-format'
$clangVersion = (& $clangFormat --version) -replace '.*version (\d+\.\d+\.\d+).*', '$1'
if (([version] $clangVersion).Major -ne 19) {
    throw "clang-format 19.x required (found $clangVersion at $clangFormat)"
}

$gersemi = Find-Tool 'gersemi'

$cppFiles = foreach ($dir in 'src', 'tests') {
    if (Test-Path "$repoRoot/$dir") {
        Get-ChildItem "$repoRoot/$dir" -Recurse -File -Include *.c, *.cpp, *.h, *.hpp |
            Where-Object { $_.FullName -notmatch '[\\/]third-party[\\/]' } |
            ForEach-Object FullName
    }
}

$cmakeFiles = @("$repoRoot/CMakeLists.txt") +
    (Get-ChildItem "$repoRoot/cmake" -Recurse -File -Include CMakeLists.txt, *.cmake | ForEach-Object FullName)

$failed = $false

if ($cppFiles) {
    Write-Host "clang-format $clangVersion on $(@($cppFiles).Count) file(s)"
    if ($Check) {
        & $clangFormat -style=file -fallback-style=none --dry-run --Werror $cppFiles
    } else {
        & $clangFormat -style=file -fallback-style=none -i $cppFiles
    }
    if ($LASTEXITCODE) { $failed = $true }
}

Write-Host "gersemi on $(@($cmakeFiles).Count) file(s)"
if ($Check) {
    & $gersemi -c --no-cache $cmakeFiles
} else {
    & $gersemi -i --no-cache $cmakeFiles
}
if ($LASTEXITCODE) { $failed = $true }

if ($failed) { throw $(if ($Check) { 'Formatting check failed' } else { 'Formatting failed' }) }
Write-Host $(if ($Check) { 'Formatting OK' } else { 'Formatted' })
