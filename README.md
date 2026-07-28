# Game Engine

A deterministic, multiplayer-first game engine built in C++ with OpenGL rendering support.

## Overview

**Core Engine**: ECS architecture, deterministic simulation, fixed timestep, job system, and platform abstraction layer.

**Rendering**: OpenGL 3.3+ with model loading (OBJ, FBX via Assimp), skeletal animation, materials, textures, and camera system.

**Development Tools**:
- **Camera System Editor**: 3D editor for camera/object animation with VIPRES (point cloud rendering, TOF simulation, mesh reconstruction)
- **Skeleton Editor**: Character skeleton editing tool

**Game Systems**: AI, crafting, economy, and inventory systems (in development).

## Quick Start

### Prerequisites
- CMake 3.22+
- C++17 compiler
- Windows: Visual Studio 2022
- Libraries in `../game_libs/` (GLEW, GLFW, GLM, Assimp)

### Build (Windows)
```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

**Output:** `build/bin/Release/game.exe`

### Build (Linux/Docker)
```bash
docker-compose run --rm build
```

## Project Structure

```
engine/          # Core engine (ECS, timestep, jobs, math)
├── core/        # ECS, deterministic math, job system
├── platform/    # Rendering, audio, input, network
└── systems/     # Game systems (inventory, crafting, AI, economy)
src/             # Game executable
camera_system/   # Camera system editor with VIPRES
editors/         # Development tools (skeleton editor)
graphics/        # Game assets
```

## Running

**Main Game:**
```powershell
cd build/bin/Release
./game.exe
```

**Camera System Editor:**
```powershell
./camera_system_editor.exe --config camera_config.json
```

**Controls:**
- WASD - Move camera
- Mouse - Look around
- ESC - Exit

## Libraries

- **GLEW 2.1.0** - OpenGL extensions
- **GLFW 3.4** - Window management (built from source)
- **GLM 1.0.2** - Math library (header-only)
- **Assimp 6.0.2** - Model loading (built from source)

First build takes 5-15 minutes (GLFW/Assimp compilation). Subsequent builds are incremental.

## CI/CD

GitHub Actions workflows:

| Workflow | Trigger | Purpose |
|----------|---------|---------|
| `ci.yml` | push/PR to `main`, `master`, `rgrigorii/menu` | Engine unit tests (Windows + Linux) |
| `game-ci.yml` | same branches + manual | Build `game.exe`, `game_server.exe` (Windows) and `game_server` (Linux) |
| `cd.yml` | tag `v*` or manual | Package Windows release zip and publish a GitHub Release |

Secrets required:

- `REPO_PAT` — token with read access to the private `RenderingEngine` submodule repo

Local CI helpers:

```powershell
# Windows: bootstrap ../game_libs when missing (used by game-ci)
./tools/ci/bootstrap_game_libs.ps1

# Windows: package a Release build
./tools/ci/package_windows_game.ps1 -BuildDir build -Config Release
```

```bash
# Linux: install native deps for game/server builds
./tools/ci/install_linux_game_deps.sh
```

## Rendering

Shaders: `engine/platform/render/shaders/`
- `shader.vert` - Vertex shader
- `shader.frag` - Fragment shader with lighting

Model loading supports: `.obj`, `.fbx`, `.dae`, `.gltf`, and more via Assimp.

## Troubleshooting

**CMake version error**: Requires CMake 3.22+ for Assimp 6.0.2

**Libraries not found**: Ensure `game_libs/` is in `../game_libs/` relative to project root

**Shader files not found**: Automatically copied during CMake configuration
