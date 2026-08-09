<#
.SYNOPSIS
    Builds every MakuTweaker++ artifact: the normal build, the single-file
    build, and the NSIS installer.

.DESCRIPTION
    Produces
      build/Release/            MakuTweaker++.exe + .com + loc/ assets/ previewimg/
      build-onefile/Release/    MakuTweaker++.exe   (self-contained, no sidecars)
      dist/MakuTweaker++-Setup.exe        installer for the normal build
      dist/MakuTweaker++-portable.exe     the single-file build

    Both configurations link the MSVC runtime statically, so nothing needs the
    Visual C++ Redistributable.

    The installer packages the *normal* build because that is the layout that
    benefits from one: the single-file build is already a single file and is
    shipped as the portable download instead.

.PARAMETER Generator
    CMake generator. Defaults to the newest installed Visual Studio.

.PARAMETER SkipOnefile
    Skip the single-file configuration.
#>
[CmdletBinding()]
param(
    [string]$Generator,
    [switch]$SkipOnefile
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

function Resolve-CMake {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    foreach ($vs in (Get-ChildItem 'C:\Program Files\Microsoft Visual Studio' -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending)) {
        foreach ($ed in 'Community', 'Professional', 'Enterprise') {
            $c = Join-Path $vs.FullName "$ed\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if (Test-Path $c) { return $c }
        }
    }
    throw 'cmake.exe not found. Install CMake or the "Desktop development with C++" workload.'
}

function Resolve-Generator {
    foreach ($vs in (Get-ChildItem 'C:\Program Files\Microsoft Visual Studio' -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending)) {
        switch ($vs.Name) {
            '18' { return 'Visual Studio 18 2026' }
            '2022' { return 'Visual Studio 17 2022' }
            '2019' { return 'Visual Studio 16 2019' }
        }
    }
    throw 'No usable Visual Studio generator found.'
}

function Resolve-MakeNsis {
    $cmd = Get-Command makensis -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    foreach ($p in "${env:ProgramFiles(x86)}\NSIS\makensis.exe", "$env:ProgramFiles\NSIS\makensis.exe") {
        if ($p -and (Test-Path $p)) { return $p }
    }
    return $null
}

function Invoke-Build {
    param([string]$CMakeExe, [string]$BuildDir, [string[]]$ConfigureArgs)

    Write-Host "==> configure $(Split-Path -Leaf $BuildDir)" -ForegroundColor Cyan
    & $CMakeExe -S $root -B $BuildDir @ConfigureArgs
    if ($LASTEXITCODE -ne 0) { throw "configure failed for $BuildDir" }

    Write-Host "==> build $(Split-Path -Leaf $BuildDir)" -ForegroundColor Cyan
    & $CMakeExe --build $BuildDir --config Release
    if ($LASTEXITCODE -ne 0) { throw "build failed for $BuildDir" }
}

$cmake = Resolve-CMake
if (-not $Generator) { $Generator = Resolve-Generator }

$buildNumber = '0'
$buildNumberFile = Join-Path $root 'BuildNumber.txt'
if (Test-Path $buildNumberFile) { $buildNumber = (Get-Content $buildNumberFile -Raw).Trim() }
$version = "5.8.$buildNumber"

Write-Host "cmake     : $cmake"
Write-Host "generator : $Generator"
Write-Host "version   : $version"

# A running copy keeps the .exe locked and the link step fails with LNK1104.
$running = Get-Process 'MakuTweaker++' -ErrorAction SilentlyContinue
if ($running) {
    throw ("MakuTweaker++ is running (PID {0}). Close it before building." -f ($running.Id -join ', '))
}

Invoke-Build -CMakeExe $cmake -BuildDir (Join-Path $root 'build') `
    -ConfigureArgs @('-G', $Generator, '-A', 'x64')

if (-not $SkipOnefile) {
    Invoke-Build -CMakeExe $cmake -BuildDir (Join-Path $root 'build-onefile') `
        -ConfigureArgs @('-G', $Generator, '-A', 'x64', '-DMAKU_ONEFILE=ON')
}

$dist = Join-Path $root 'dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
Get-ChildItem $dist -File -ErrorAction SilentlyContinue | Remove-Item -Force

# --- installer (normal build) ---
$makensis = Resolve-MakeNsis
if ($makensis) {
    Write-Host '==> installer' -ForegroundColor Cyan
    $sourceDir = Join-Path $root 'build\Release'
    $outFile = Join-Path $dist 'MakuTweaker++-Setup.exe'
    & $makensis /V2 "/DSOURCE_DIR=$sourceDir" "/DAPP_VERSION=$version" "/DOUTPUT_FILE=$outFile" `
        (Join-Path $root 'installer\MakuTweaker.nsi')
    if ($LASTEXITCODE -ne 0) { throw 'makensis failed' }
} else {
    Write-Warning 'makensis.exe not found - skipping the installer. Install NSIS from https://nsis.sourceforge.io'
}

# --- portable single file ---
$portable = Join-Path $root 'build-onefile\Release\MakuTweaker++.exe'
if (Test-Path $portable) {
    Copy-Item $portable (Join-Path $dist 'MakuTweaker++-portable.exe') -Force
    $cli = Join-Path $root 'build-onefile\Release\MakuTweaker++.com'
    if (Test-Path $cli) { Copy-Item $cli $dist -Force }
}

Write-Host ''
Write-Host 'Artifacts in dist\:' -ForegroundColor Green
Get-ChildItem $dist -File | ForEach-Object {
    '  {0,-32} {1,8:N0} KB' -f $_.Name, ($_.Length / 1KB)
}
