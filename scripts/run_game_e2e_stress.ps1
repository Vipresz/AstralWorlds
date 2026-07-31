# Game-level e2e: game_server + GameTests (and optional game.exe subprocess).
#
# Fast path (default):
#   .\scripts\run_game_e2e_stress.ps1
#
# Multi-client stress:
#   .\scripts\run_game_e2e_stress.ps1 -StressClients 16
#
# Also run game.exe subprocess validation:
#   .\scripts\run_game_e2e_stress.ps1 -RunGameSubprocess

param(
    [string]$BuildDir = "",
    [string]$Config = "Release",
    [int]$StressClients = 0,
    [switch]$RunGameSubprocess,
    [switch]$AllowPartialSuccess
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..")
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot "build"
}

$BinDir = Join-Path $BuildDir "bin\$Config"
$GameTests = Join-Path $BinDir "GameTests.exe"
$GameExe = Join-Path $BinDir "game.exe"
$GameServer = Join-Path $BinDir "game_server.exe"

function Ensure-Built {
    if (-not (Test-Path $GameTests)) {
        Write-Host "Configuring and building GameTests..."
        cmake -S $RepoRoot -B $BuildDir -DBUILD_GAME=ON -DBUILD_GAME_TESTS=ON -DBUILD_ENGINE_TESTS=OFF -DBUILD_SKELETON_EDITOR=OFF
        cmake --build $BuildDir --config $Config --target GameTests GameServer Game --parallel
    }
}

Ensure-Built

if (-not (Test-Path $GameTests)) { throw "GameTests.exe missing at $GameTests" }
if (-not (Test-Path $GameServer)) { throw "game_server.exe missing at $GameServer" }

$serverPort = 28000 + ([int](Get-Random -Maximum 5000))
$serverProc = Start-Process -FilePath $GameServer -ArgumentList @("--port", "$serverPort") -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 2

$overallExit = 0
try {
    Write-Host "Running GameTests game network e2e..."
    Push-Location $BinDir
    $ciFilter = "GameNetworkE2ETest.ConnectLoadCharacterAndMeasureMovementAck:GameNetworkE2ETest.BroadcastsChatBetweenPlayers:CharacterLoadE2ETest.*:ChatInputFocusTest.*:GameMenusManifestTest.*"
    & $GameTests --gtest_filter=$ciFilter
    if ($LASTEXITCODE -ne 0) { $overallExit = $LASTEXITCODE }

    if ($StressClients -gt 0) {
        Write-Host "Running multi-client stress with $StressClients clients..."
        $env:GAME_RUN_E2E_STRESS = "1"
        $env:GAME_E2E_STRESS_CLIENTS = "$StressClients"
        & $GameTests --gtest_filter=GameNetworkE2ETest.MultiClientLatencyStress
        if ($LASTEXITCODE -ne 0 -and -not $AllowPartialSuccess) { $overallExit = $LASTEXITCODE }
        Remove-Item Env:GAME_RUN_E2E_STRESS -ErrorAction SilentlyContinue
        Remove-Item Env:GAME_E2E_STRESS_CLIENTS -ErrorAction SilentlyContinue
    }

    if ($RunGameSubprocess) {
        if (-not (Test-Path $GameExe)) { throw "game.exe missing at $GameExe" }
        Write-Host "Running game.exe --e2e-test subprocess against port $serverPort..."
        $reportPath = Join-Path $BinDir "e2e_report.json"
        if (Test-Path $reportPath) { Remove-Item $reportPath -Force }
        $gameArgs = @(
            "--host", "127.0.0.1",
            "--port", "$serverPort",
            "--name", "E2E_Player",
            "--e2e-test",
            "--e2e-seconds", "6",
            "--e2e-report", $reportPath
        )
        $gameProc = Start-Process -FilePath $GameExe -ArgumentList $gameArgs -PassThru -Wait -WorkingDirectory $BinDir
        if ($gameProc.ExitCode -ne 0) { throw "game.exe --e2e-test failed with exit code $($gameProc.ExitCode)" }
        if (-not (Test-Path $reportPath)) { throw "Missing e2e report at $reportPath" }
        Write-Host "game.exe e2e report:"
        Get-Content $reportPath
    }
}
finally {
    Pop-Location -ErrorAction SilentlyContinue
    if ($serverProc -and -not $serverProc.HasExited) {
        Stop-Process -Id $serverProc.Id -Force -ErrorAction SilentlyContinue
    }
}

if ($overallExit -ne 0) { exit $overallExit }
Write-Host "Game e2e stress run completed successfully."
