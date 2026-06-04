$buildFilePath = Join-Path $PSScriptRoot "..\BuildNumber.txt"
$currentBuildNumber = [int](Get-Content $buildFilePath -ErrorAction SilentlyContinue)
if (-not $currentBuildNumber) { $currentBuildNumber = 0 }
$currentBuildNumber++
$currentBuildNumber | Set-Content $buildFilePath
Write-Host "BuildNumber=$currentBuildNumber"
