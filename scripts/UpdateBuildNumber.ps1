# Increments BuildNumber.txt (tracked in git; last digit of version 5.8.N).
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$py = Get-Command python -ErrorAction SilentlyContinue
if (-not $py) { $py = Get-Command python3 -ErrorAction SilentlyContinue }
if (-not $py) { throw "Python not found (required for bump_build.py)" }
& $py.Source (Join-Path $PSScriptRoot "bump_build.py") --root $root
if ($env:GITHUB_ACTIONS -eq "true") {
    $n = Get-Content (Join-Path $root "BuildNumber.txt") -Raw
    Write-Host "##vso[task.setvariable variable=BuildNumber]$($n.Trim())"
}
