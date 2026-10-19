# Game e2e stress profile sweep (64 / 256 / 1048 players).
#
# Usage:
#   .\scripts\run_game_e2e_stress_profile.ps1
#   .\scripts\run_game_e2e_stress_profile.ps1 -Build
#
# Outputs:
#   scripts\logs\game_e2e_stress_profile_YYYYMMDD_HHMMSS.txt
#   scripts\logs\game_e2e_stress_profile_latest.txt

[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug")]
    [string] $Config = "Release",

    [string] $BuildDir = "",

    [switch] $Build,

    [int[]] $PlayerCounts = @(64, 256, 1048),

    [switch] $AllowPartialSuccess
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..")
$LogsDir = Join-Path $ScriptDir "logs"

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot "build"
}

$BinDir = Join-Path $BuildDir "bin\$Config"
$GameTests = Join-Path $BinDir "GameTests.exe"
$ScaleConfig = Join-Path $RepoRoot "RenderingEngine\config\join_load_stress_scale.json"

if ($Build -or -not (Test-Path $GameTests)) {
    Write-Host "Building GameTests..."
    cmake -S $RepoRoot -B $BuildDir -DBUILD_GAME=ON -DBUILD_GAME_TESTS=ON -DBUILD_ENGINE_TESTS=OFF -DBUILD_SKELETON_EDITOR=OFF
    cmake --build $BuildDir --config $Config --target GameTests --parallel
}

if (-not (Test-Path $GameTests)) { throw "GameTests.exe missing at $GameTests" }
if (-not (Test-Path $ScaleConfig)) { throw "Scale config missing at $ScaleConfig" }

if (-not (Test-Path $LogsDir)) {
    New-Item -ItemType Directory -Path $LogsDir | Out-Null
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$txtPath = Join-Path $LogsDir "game_e2e_stress_profile_$timestamp.txt"
$latestTxtPath = Join-Path $LogsDir "game_e2e_stress_profile_latest.txt"

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("AstralWorlds - game e2e stress profile")
$lines.Add("Started: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')")
$lines.Add("Scale config: $ScaleConfig")
$lines.Add("Player counts: $($PlayerCounts -join ', ')")
$lines.Add("")
$lines.Add(("Players  Joined   CharOK   MoveOK   ConnectP95   CharP95      MoveP95      BurstMs").PadRight(96))
$lines.Add(("-------- -------- -------- -------- ------------ ------------ ------------ ----------"))

$overallExit = 0
Push-Location $BinDir
try {
    $env:GAME_E2E_STRESS_PROFILE = "1"
    $env:GAME_E2E_SCALE_CONFIG = $ScaleConfig

    foreach ($count in $PlayerCounts) {
        Write-Host ""
        Write-Host "=== Game e2e stress: $count players ==="
        $env:GAME_E2E_PLAYER_COUNT = "$count"
        $prevErrorAction = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        try {
            $output = & $GameTests --gtest_filter=GameNetworkE2ETest.ConcurrentStressProfile 2>&1 | Out-String
            $exitCode = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $prevErrorAction
        }
        Write-Host $output

        if ($exitCode -ne 0) {
            if ($AllowPartialSuccess) {
                Write-Warning "Player count $count failed with exit code $exitCode"
                $overallExit = $exitCode
            } else {
                throw "Game e2e stress failed for $count players (exit $exitCode)"
            }
        }

        $profileLine = ($output -split "`n" | Where-Object { $_ -match '\[GameE2EProfile\]' } | Select-Object -Last 1)
        if (-not $profileLine) {
            if ($AllowPartialSuccess) {
                Write-Warning "Missing [GameE2EProfile] line for $count players"
                continue
            }
            throw "Missing [GameE2EProfile] line for $count players"
        }

        $jsonText = $profileLine -replace '.*\[GameE2EProfile\]\s*', ''
        $profile = $jsonText | ConvertFrom-Json

        $row = "{0,8} {1,8} {2,8} {3,8} {4,12:N1} {5,12:N1} {6,12:N1} {7,10:N1}" -f `
            $profile.players, $profile.joined, $profile.character_ok, $profile.movement_ok, `
            $profile.connect_p95_ms, $profile.character_load_p95_ms, $profile.movement_p95_ms, $profile.burst_wall_ms
        $lines.Add($row)

        $lines.Add("")
        $lines.Add("Players ${count}: joined $($profile.joined)/$($profile.players), character_ok $($profile.character_ok), movement_ok $($profile.movement_ok)")
        $lines.Add("  burst_wall_ms=$([math]::Round($profile.burst_wall_ms, 3)) character_mesh_load=$($profile.character_mesh_load)")
        $lines.Add("  connect_p95_ms=$([math]::Round($profile.connect_p95_ms, 3)) character_load_p95_ms=$([math]::Round($profile.character_load_p95_ms, 3)) movement_p95_ms=$([math]::Round($profile.movement_p95_ms, 3))")
        $lines.Add("")
    }
}
finally {
    Pop-Location
    Remove-Item Env:GAME_E2E_STRESS_PROFILE -ErrorAction SilentlyContinue
    Remove-Item Env:GAME_E2E_PLAYER_COUNT -ErrorAction SilentlyContinue
    Remove-Item Env:GAME_E2E_SCALE_CONFIG -ErrorAction SilentlyContinue
}

$lines.Add("Finished: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')")
$lines.Add("Overall exit code: $overallExit")

$text = ($lines -join [Environment]::NewLine)
$text | Set-Content -Path $txtPath -Encoding UTF8
$text | Set-Content -Path $latestTxtPath -Encoding UTF8

Write-Host ""
Write-Host "Game e2e stress profile summary"
Write-Host $text
Write-Host ""
Write-Host "Text report: $txtPath"
Write-Host "Latest report: $latestTxtPath"

exit $overallExit
