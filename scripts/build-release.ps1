#Requires -Version 5.1
<#
.SYNOPSIS
  Configure and build MakuTweaker++ Release (CMake preset).

.EXAMPLE
  .\scripts\build-release.ps1
  .\scripts\build-release.ps1 -Ninja
#>
param(
    [switch]$Ninja,
    [switch]$SkipBuildNumber
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

if (-not $SkipBuildNumber) {
    & "$Root\scripts\UpdateBuildNumber.ps1"
}

if ($Ninja) {
    cmake --preset windows-x64-release
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    cmake --build --preset ninja-release
    $exe = Join-Path $Root 'build\ninja-release\MakuTweaker++.exe'
} else {
    # CMake 3.29+: one-shot workflow; fallback for older CMake.
    $workflowOk = $false
    try {
        cmake --workflow --preset release 2>$null
        if ($LASTEXITCODE -eq 0) { $workflowOk = $true }
    } catch {}

    if (-not $workflowOk) {
        cmake --preset windows-x64
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        cmake --build --preset release
    }
    $exe = Join-Path $Root 'build\Release\MakuTweaker++.exe'
}

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if (-not (Test-Path $exe)) {
    Write-Error "Expected output not found: $exe"
}
Write-Host "Release build OK: $exe" -ForegroundColor Green
