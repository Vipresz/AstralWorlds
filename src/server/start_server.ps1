# Start the AstralWorlds multiplayer relay server.
# Run from repo root or this folder after building GameServer.

param(
    [int]$Port = 7777,
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$serverExe = Join-Path $repoRoot "build\bin\$Config\game_server.exe"

if (-not (Test-Path $serverExe)) {
    Write-Host "game_server.exe not found at $serverExe" -ForegroundColor Yellow
    Write-Host "Building GameServer ($Config)..." -ForegroundColor Cyan

    $buildDir = Join-Path $repoRoot "build"
    if (-not (Test-Path $buildDir)) {
        New-Item -ItemType Directory -Path $buildDir | Out-Null
        Push-Location $buildDir
        cmake .. -G "Visual Studio 17 2022" -A x64
        Pop-Location
    }

    cmake --build $buildDir --config $Config --target GameServer
    if (-not (Test-Path $serverExe)) {
        throw "Build failed - game_server.exe still missing."
    }
}

Write-Host "Starting AstralWorlds server on port $Port ($Config)..." -ForegroundColor Green
Write-Host "Clients: game.exe --name YourName --host 127.0.0.1 --port $Port" -ForegroundColor DarkGray

& $serverExe --port $Port
