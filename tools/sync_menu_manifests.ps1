# Sync menu manifests and character stats from src/ to build output (no rebuild required).
# Usage:
#   .\tools\sync_menu_manifests.ps1
#   .\tools\sync_menu_manifests.ps1 -WithAssets
#   .\tools\sync_menu_manifests.ps1 -WithCharacterData
#   .\tools\sync_menu_manifests.ps1 -AllMenus -Config Debug
#   .\tools\sync_menu_manifests.ps1 -SkipCharacterStats

param(
    [ValidateSet("Debug", "Release", "all")]
    [string]$Config = "all",
    [switch]$WithAssets,
    [switch]$AllMenus,
    [switch]$WithCharacterData,
    [switch]$SkipCharacterStats,
    [switch]$DryRun
)

$repoRoot = Split-Path -Parent $PSScriptRoot
$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) {
    $python = Get-Command py -ErrorAction SilentlyContinue
}

if (-not $python) {
    Write-Error "Python not found. Install Python or run sync_menu_manifests.py manually."
    exit 1
}

# Always sync shared menu assets (includes hud/) so layout+art stay in sync with manifests.
$argsList = @("$repoRoot\tools\sync_menu_manifests.py", "--config", $Config, "--with-assets")
if ($WithAssets) { } # already enabled by default
if ($AllMenus) { $argsList += "--all-menus" }
if ($WithCharacterData) { $argsList += "--with-character-data" }
if ($SkipCharacterStats) { $argsList += "--skip-character-stats" }
if ($DryRun) { $argsList += "-n" }

& $python.Source @argsList
exit $LASTEXITCODE
