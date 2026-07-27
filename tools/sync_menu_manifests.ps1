# Sync menu manifests from src/ to build output (no rebuild required).
# Usage:
#   .\tools\sync_menu_manifests.ps1
#   .\tools\sync_menu_manifests.ps1 -WithAssets
#   .\tools\sync_menu_manifests.ps1 -AllMenus -Config Debug

param(
    [ValidateSet("Debug", "Release", "all")]
    [string]$Config = "all",
    [switch]$WithAssets,
    [switch]$AllMenus,
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

$argsList = @("$repoRoot\tools\sync_menu_manifests.py", "--config", $Config)
if ($WithAssets) { $argsList += "--with-assets" }
if ($AllMenus) { $argsList += "--all-menus" }
if ($DryRun) { $argsList += "-n" }

& $python.Source @argsList
exit $LASTEXITCODE
