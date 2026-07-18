# Re-launches Profile-MakuTweaker.ps1 with Administrator rights (UAC prompt).
param(
    [int] $DurationSec = 25,
    [string] $ExePath = "",
    [string] $Profile = "CPU"
)

$script = Join-Path $PSScriptRoot "Profile-MakuTweaker.ps1"
if (-not (Test-Path -LiteralPath $script)) {
    Write-Error "Missing: $script"
}

$psArgs = @(
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", $script,
    "-DurationSec", $DurationSec
)
if ($ExePath) { $psArgs += @("-ExePath", $ExePath) }
if ($Profile) { $psArgs += @("-Profile", $Profile) }

Start-Process -FilePath "powershell.exe" -Verb RunAs -ArgumentList $psArgs
Write-Host "UAC: approve elevation to capture profiles/maku_profile.etl"
