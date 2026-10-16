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

$RunnerScript = Join-Path $PSScriptRoot "run_backend_unit_tests.ps1"
& $RunnerScript `
    -Config $Config `
    -BuildDir $BuildDir `
    -Suite $Suite `
    -Filter $Filter `
    -NoBuild:$NoBuild `
    -IncludeStress:$IncludeStress `
    -ListTests:$ListTests `
    -Jobs $Jobs
exit $LASTEXITCODE
