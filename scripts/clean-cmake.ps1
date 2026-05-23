#Requires -Version 5.1
<#
.SYNOPSIS
  Remove stale CMake / Visual Studio build caches (fixes path-mismatch errors).

.EXAMPLE
  .\scripts\clean-cmake.ps1
#>
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$paths = @(
    'build',
    'out\build',
    'out\install',
    '.vs'
)

foreach ($rel in $paths) {
    $full = Join-Path $Root $rel
    if (-not (Test-Path $full)) { continue }
    Write-Host "Removing $rel ..."
    try {
        Remove-Item -LiteralPath $full -Recurse -Force -ErrorAction Stop
    } catch {
        Write-Warning "Could not remove $rel (close Visual Studio and retry): $_"
    }
}

Write-Host "CMake cache cleared. In Visual Studio: Project -> Delete Cache and Reconfigure." -ForegroundColor Green
