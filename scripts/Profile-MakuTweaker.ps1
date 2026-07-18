#Requires -RunAsAdministrator
<#
.SYNOPSIS
  Captures a WPR CPU profile for MakuTweaker++.

.DESCRIPTION
  Writes: profiles/maku_profile.etl (repo root, next to scripts/).
  Open the .etl in Windows Performance Analyzer (WPA) from the Windows SDK.

.PARAMETER DurationSec
  Seconds to record after launching the app.

.PARAMETER ExePath
  Path to MakuTweaker++.exe (default: build-onefile Release).

.PARAMETER Profile
  WPR built-in profile name (see: wpr -profiles). Default: CPU
#>
param(
    [int] $DurationSec = 25,
    [string] $ExePath = "",
    [string] $Profile = "CPU"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path $PSScriptRoot -Parent

function Invoke-Wpr {
    param(
        [Parameter(Mandatory)]
        [string[]] $WprArgs,
        [switch] $AllowNonZeroExit
    )
    $p = Start-Process -FilePath $WprExe -ArgumentList $WprArgs -Wait -PassThru `
        -NoNewWindow -RedirectStandardOutput "$env:TEMP\maku_wpr_out.txt" `
        -RedirectStandardError "$env:TEMP\maku_wpr_err.txt"
    if (-not $AllowNonZeroExit -and $p.ExitCode -ne 0) {
        $err = Get-Content -LiteralPath "$env:TEMP\maku_wpr_err.txt" -Raw -ErrorAction SilentlyContinue
        $out = Get-Content -LiteralPath "$env:TEMP\maku_wpr_out.txt" -Raw -ErrorAction SilentlyContinue
        $detail = ($err, $out | Where-Object { $_ }) -join "`n"
        $cmd = "wpr " + ($WprArgs -join ' ')
        throw "$cmd failed (exit $($p.ExitCode)).`n$detail"
    }
    return $p.ExitCode
}

if (-not $ExePath) {
    $candidates = @(
        (Join-Path $RepoRoot "build-onefile\Release\MakuTweaker++.exe"),
        (Join-Path $RepoRoot "build\Release\MakuTweaker++.exe"),
        (Join-Path $RepoRoot "build\Release\MakuTweaker.exe")
    )
    foreach ($c in $candidates) {
        if (Test-Path -LiteralPath $c) {
            $ExePath = $c
            break
        }
    }
}

if (-not $ExePath -or -not (Test-Path -LiteralPath $ExePath)) {
    Write-Error "MakuTweaker++.exe not found. Build Release first or pass -ExePath."
}

$WprExe = "${env:ProgramFiles(x86)}\Windows Kits\10\Windows Performance Toolkit\wpr.exe"
if (-not (Test-Path -LiteralPath $WprExe)) {
    $WprExe = Join-Path $env:SystemRoot "System32\wpr.exe"
}
if (-not (Test-Path -LiteralPath $WprExe)) {
    Write-Error "wpr.exe not found. Install Windows Performance Toolkit (Windows SDK)."
}

$profilesDir = Join-Path $RepoRoot "profiles"
New-Item -ItemType Directory -Force -Path $profilesDir | Out-Null
$OutEtl = Join-Path $profilesDir "maku_profile.etl"

Write-Host "WPR: $WprExe"
Write-Host "App: $ExePath"
Write-Host "Out: $OutEtl"
Write-Host "Profile: $Profile"
Write-Host "Duration: ${DurationSec}s"

# No active session is normal — do not treat stderr/exit as fatal.
Invoke-Wpr -WprArgs @("-cancel") -AllowNonZeroExit | Out-Null

if (Test-Path -LiteralPath $OutEtl) {
    Remove-Item -LiteralPath $OutEtl -Force -ErrorAction SilentlyContinue
}

Invoke-Wpr -WprArgs @("-start", $Profile, "-filemode") | Out-Null

$proc = $null
try {
    $proc = Start-Process -FilePath $ExePath -PassThru
    Write-Host "Recording... (reproduce: UWP list, WinInfo, Monitor)"
    Start-Sleep -Seconds $DurationSec
}
finally {
    if ($proc -and -not $proc.HasExited) {
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    }
    try {
        Invoke-Wpr -WprArgs @("-stop", $OutEtl) | Out-Null
    }
    catch {
        Write-Warning $_.Exception.Message
    }
    Invoke-Wpr -WprArgs @("-cancel") -AllowNonZeroExit | Out-Null
}

if (-not (Test-Path -LiteralPath $OutEtl)) {
    Write-Error "ETL was not created: $OutEtl"
}

$sizeMb = [math]::Round((Get-Item -LiteralPath $OutEtl).Length / 1MB, 2)
Write-Host "Done: $OutEtl ($sizeMb MB)"
Write-Host "Open with WPA (Windows Performance Analyzer) and load this file."
