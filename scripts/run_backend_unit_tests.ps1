param(
    [ValidateSet("Release", "Debug", "RelWithDebInfo", "MinSizeRel")]
    [Alias("Configuration")]
    [string]$Config = "Release",

    [string]$BuildDir = "build",

    [ValidateSet("All", "Backend", "Engine", "Game")]
    [string]$Suite = "All",

    [string]$Filter = "",

    [switch]$NoBuild,

    [switch]$IncludeStress,

    [switch]$ListTests,

    [int]$Jobs = 4
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$BuildPath = if ([System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir
} else {
    Join-Path $RepoRoot $BuildDir
}

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " AstralWorlds Backend & Unit Test Runner" -ForegroundColor Cyan
Write-Host " Configuration : $Config"
Write-Host " Suite         : $Suite"
Write-Host " Build Dir     : $BuildPath"
if (-not [string]::IsNullOrWhiteSpace($Filter)) {
    Write-Host " GTest Filter  : $Filter"
}
Write-Host "============================================================" -ForegroundColor Cyan

# Determine which targets to build and run
$RunEngine = ($Suite -eq "All" -or $Suite -eq "Backend" -or $Suite -eq "Engine")
$RunGame   = ($Suite -eq "All" -or $Suite -eq "Backend" -or $Suite -eq "Game")

$TargetsToBuild = @()
if ($RunEngine) { $TargetsToBuild += "EngineTests" }
if ($RunGame)   { $TargetsToBuild += "GameTests" }

if (-not $NoBuild) {
    $CacheFile = Join-Path $BuildPath "CMakeCache.txt"
    if (-not (Test-Path $CacheFile)) {
        Write-Host "`n[1/2] Configuring CMake in $BuildPath..." -ForegroundColor Yellow
        & cmake -S $RepoRoot -B $BuildPath -DBUILD_ENGINE_TESTS=ON -DBUILD_GAME_TESTS=ON -DBUILD_GAME=ON
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configuration failed with exit code $LASTEXITCODE"
        }
    }

    Write-Host "`n[1/2] Building test target(s): $($TargetsToBuild -join ', ') ($Config)..." -ForegroundColor Yellow
    $BuildArgs = @("--build", $BuildPath, "--config", $Config, "--target") + $TargetsToBuild + @("--parallel", "$Jobs")
    & cmake @BuildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed with exit code $LASTEXITCODE"
    }
} else {
    Write-Host "`n[1/2] Skipping build (-NoBuild specified)." -ForegroundColor DarkGray
}

$BinDir = Join-Path $BuildPath (Join-Path "bin" $Config)
if (-not (Test-Path $BinDir)) {
    $FallbackBinDir = Join-Path $BuildPath "bin"
    if (Test-Path $FallbackBinDir) {
        $BinDir = $FallbackBinDir
    }
}

# Handle optional stress test environment variables
$PrevEngineJoinStress = $env:ENGINE_RUN_JOIN_LOAD_STRESS
$PrevEngineJoinProfile = $env:ENGINE_JOIN_LOAD_STRESS_PROFILE
$PrevGameE2eStress = $env:GAME_RUN_E2E_STRESS
$PrevGameE2eProfile = $env:GAME_E2E_STRESS_PROFILE

if ($IncludeStress) {
    $env:ENGINE_RUN_JOIN_LOAD_STRESS = "1"
    $env:GAME_RUN_E2E_STRESS = "1"
}

$Results = @()
$OverallExitCode = 0

try {
    Write-Host "`n[2/2] Running unit test suite(s)..." -ForegroundColor Yellow

    foreach ($Target in $TargetsToBuild) {
        $ExePath = Join-Path $BinDir "$Target.exe"
        if (-not (Test-Path $ExePath)) {
            $ExePath = Join-Path $BinDir $Target
        }
        if (-not (Test-Path $ExePath)) {
            throw "Test executable not found at $ExePath. Run without -NoBuild first."
        }

        $GTestArgs = @("--gtest_color=yes")
        if ($ListTests) {
            $GTestArgs += "--gtest_list_tests"
        }

        $EffectiveFilter = $Filter
        if ([string]::IsNullOrWhiteSpace($EffectiveFilter) -and $Suite -eq "Backend") {
            if ($Target -eq "EngineTests") {
                # Backend-focused filter for EngineTests (networking, server, gateway, simulator, replication, ECS, crypto/auth, db)
                $EffectiveFilter = "*Net*:*Socket*:*Udp*:*Server*:*Gateway*:*Authoritative*:*Shard*:*Migration*:*Scale*:*Layer*:*Zone*:*CellLayer*:*Replication*:*Trade*:*Ownership*:*Chat*:*Tls*:*Password*:*Credential*:*Interest*:*RateLimiter*:*ECS*"
            } elseif ($Target -eq "GameTests") {
                # All GameTests (database, inventory, skills, replication, configs, e2e server/client)
                $EffectiveFilter = "*"
            }
        }

        if (-not [string]::IsNullOrWhiteSpace($EffectiveFilter)) {
            $GTestArgs += "--gtest_filter=$EffectiveFilter"
        }

        Write-Host "`n------------------------------------------------------------" -ForegroundColor DarkCyan
        Write-Host " Running $Target ($ExePath)" -ForegroundColor Cyan
        if (-not [string]::IsNullOrWhiteSpace($EffectiveFilter)) {
            Write-Host " Filter: $EffectiveFilter" -ForegroundColor DarkGray
        }
        Write-Host "------------------------------------------------------------" -ForegroundColor DarkCyan

        $Stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        Push-Location $BinDir
        try {
            & $ExePath @GTestArgs
            $ExitCode = $LASTEXITCODE
        } finally {
            Pop-Location
        }
        $Stopwatch.Stop()

        $Status = if ($ExitCode -eq 0) { "PASSED" } else { "FAILED" }
        if ($ExitCode -ne 0) {
            $OverallExitCode = $ExitCode
        }

        $Results += [PSCustomObject]@{
            Target   = $Target
            Status   = $Status
            ExitCode = $ExitCode
            Seconds  = [Math]::Round($Stopwatch.Elapsed.TotalSeconds, 2)
        }
    }
} finally {
    $env:ENGINE_RUN_JOIN_LOAD_STRESS = $PrevEngineJoinStress
    $env:ENGINE_JOIN_LOAD_STRESS_PROFILE = $PrevEngineJoinProfile
    $env:GAME_RUN_E2E_STRESS = $PrevGameE2eStress
    $env:GAME_E2E_STRESS_PROFILE = $PrevGameE2eProfile
}

Write-Host "`n============================================================" -ForegroundColor Cyan
Write-Host " Test Execution Summary" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
foreach ($Row in $Results) {
    $Color = if ($Row.ExitCode -eq 0) { "Green" } else { "Red" }
    Write-Host ("  {0,-15} : {1,-8} ({2}s)" -f $Row.Target, $Row.Status, $Row.Seconds) -ForegroundColor $Color
}
Write-Host "============================================================" -ForegroundColor Cyan

exit $OverallExitCode
