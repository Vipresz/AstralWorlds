# Downloads Windows game_libs dependencies when expected headers/libs are missing (CI bootstrap).
param(
    [string]$GameLibsPath = ""
)

$ErrorActionPreference = "Stop"

function Get-DefaultGameLibsPath {
    $repoRoot = Resolve-Path (Join-Path $PSScriptRoot "../..")
    return (Join-Path (Split-Path $repoRoot -Parent) "game_libs")
}

function Ensure-Dir([string]$Path) {
    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Expand-Zip([string]$ZipPath, [string]$Destination) {
    Ensure-Dir $Destination
    Expand-Archive -Path $ZipPath -DestinationPath $Destination -Force
}

if ([string]::IsNullOrWhiteSpace($GameLibsPath)) {
    $GameLibsPath = Get-DefaultGameLibsPath
}

Write-Host "Bootstrapping game_libs at $GameLibsPath"
Ensure-Dir $GameLibsPath

$cacheDir = Join-Path $env:TEMP "astralworlds-game-libs-cache"
Ensure-Dir $cacheDir

$downloads = @(
    @{
        Name = "glew"
        Url = "https://github.com/nigels-com/glew/releases/download/glew-2.1.0/glew-2.1.0-win32.zip"
        Zip = Join-Path $cacheDir "glew-2.1.0-win32.zip"
        Expect = "glew-2.1.0/include/GL/glew.h"
    },
    @{
        Name = "glfw"
        Url = "https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip"
        Zip = Join-Path $cacheDir "glfw-3.4.zip"
        Expect = "glfw-3.4/include/GLFW/glfw3.h"
    },
    @{
        Name = "glm"
        Url = "https://github.com/g-truc/glm/releases/download/1.0.2/glm-1.0.2.zip"
        Zip = Join-Path $cacheDir "glm-1.0.2.zip"
        # Platform CMake expects game_libs/glm-1.0.2/glm/glm.hpp
        Expect = "glm-1.0.2/glm/glm.hpp"
    },
    @{
        Name = "assimp"
        Url = "https://github.com/assimp/assimp/archive/refs/tags/v6.0.2.zip"
        Zip = Join-Path $cacheDir "assimp-6.0.2.zip"
        Expect = "assimp-6.0.2/include/assimp/Importer.hpp"
    }
)

foreach ($item in $downloads) {
    $expectedPath = Join-Path $GameLibsPath $item.Expect
    if (Test-Path $expectedPath) {
        Write-Host "  skip $($item.Name) (already at $($item.Expect))"
        continue
    }

    if (-not (Test-Path $item.Zip)) {
        Write-Host "  downloading $($item.Name)..."
        Invoke-WebRequest -Uri $item.Url -OutFile $item.Zip
    }

    Write-Host "  extracting $($item.Name)..."
    $extractRoot = Join-Path $cacheDir ("extract-" + $item.Name)
    if (Test-Path $extractRoot) {
        Remove-Item -Recurse -Force $extractRoot
    }
    Expand-Zip -ZipPath $item.Zip -Destination $extractRoot

    if ($item.Name -eq "assimp") {
        $source = Join-Path $extractRoot "assimp-6.0.2"
        if (-not (Test-Path $source)) {
            $source = Get-ChildItem $extractRoot -Directory | Select-Object -First 1 -ExpandProperty FullName
        }
        $dest = Join-Path $GameLibsPath "assimp-6.0.2"
        if (Test-Path $dest) { Remove-Item -Recurse -Force $dest }
        Move-Item $source $dest
    } elseif ($item.Name -eq "glm") {
        # Official zip may unpack as glm/ or glm-1.0.2/; normalize to glm-1.0.2/glm/glm.hpp
        $destRoot = Join-Path $GameLibsPath "glm-1.0.2"
        if (Test-Path $destRoot) { Remove-Item -Recurse -Force $destRoot }

        $hdr = Get-ChildItem $extractRoot -Recurse -Filter "glm.hpp" | Select-Object -First 1
        if (-not $hdr) { throw "GLM zip did not contain glm.hpp" }

        $glmFolder = $hdr.Directory.FullName
        if ($hdr.Directory.Name -ne "glm") {
            # Headers dumped flat — wrap as glm-1.0.2/glm/
            Ensure-Dir (Join-Path $destRoot "glm")
            Get-ChildItem $glmFolder | ForEach-Object {
                Copy-Item $_.FullName (Join-Path $destRoot "glm") -Recurse -Force
            }
        } else {
            Ensure-Dir $destRoot
            Copy-Item $glmFolder (Join-Path $destRoot "glm") -Recurse -Force
        }
    } else {
        Get-ChildItem $extractRoot | ForEach-Object {
            $dest = Join-Path $GameLibsPath $_.Name
            if (Test-Path $dest) { Remove-Item -Recurse -Force $dest }
            Move-Item $_.FullName $dest
        }
    }

    if (-not (Test-Path $expectedPath)) {
        throw "Bootstrap failed for $($item.Name): expected $expectedPath"
    }
    Write-Host "  ok $($item.Name) -> $($item.Expect)"
}

$required = @(
    "glew-2.1.0/include/GL/glew.h",
    "glew-2.1.0/lib/Release/x64/glew32.lib",
    "glfw-3.4/include/GLFW/glfw3.h",
    "glm-1.0.2/glm/glm.hpp",
    "assimp-6.0.2/include/assimp/Importer.hpp"
)
foreach ($rel in $required) {
    $path = Join-Path $GameLibsPath $rel
    if (-not (Test-Path $path)) {
        throw "Missing required dependency: $path"
    }
}

Write-Host "game_libs bootstrap complete."
