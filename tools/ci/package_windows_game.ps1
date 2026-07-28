# Packages a Windows game build directory into a release zip.
param(
    [string]$BuildDir = "build",
    [string]$Config = "Release",
    [string]$OutputZip = "dist/astral-worlds-windows.zip"
)

$ErrorActionPreference = "Stop"

$binDir = Join-Path $BuildDir "bin/$Config"
if (-not (Test-Path (Join-Path $binDir "game.exe"))) {
    throw "game.exe not found in $binDir"
}

$distDir = Split-Path $OutputZip -Parent
if ($distDir -and -not (Test-Path $distDir)) {
    New-Item -ItemType Directory -Path $distDir -Force | Out-Null
}

if (Test-Path $OutputZip) {
    Remove-Item -Force $OutputZip
}

Write-Host "Packaging $binDir -> $OutputZip"
Compress-Archive -Path (Join-Path $binDir "*") -DestinationPath $OutputZip -Force
Write-Host "Created $OutputZip"
