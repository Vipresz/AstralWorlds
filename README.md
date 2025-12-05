# Game Engine

A deterministic, multiplayer-first game engine built in C++ with OpenGL rendering support.

## Project Structure

```
engine/
├── core/          # Core engine systems (ECS, timestep, jobs, math)
├── systems/       # Game systems (inventory, crafting, AI, economy)
├── platform/      # Platform layer (rendering, audio, input, network)
│   └── render/    # OpenGL rendering system
└── examples/      # Example code
src/               # Game executable source
graphics/          # Game assets (models, textures, animations)
game_libs/         # External libraries (GLEW, GLFW, GLM, Assimp)
CMakeLists.txt     # Root build configuration
```

## Quick Start

### Prerequisites

- **CMake 3.22+** (required for Assimp 6.0.2)
- **C++17 compatible compiler**
- **Windows**: Visual Studio 2022 (or build tools)
- **Linux**: GCC/Clang with build-essential

### Libraries Location

The build system expects libraries in `../game_libs/` (relative to project root):
- `glew-2.1.0/` - GLEW (OpenGL extensions)
- `glfw-3.4/` - GLFW (window management, built from source)
- `glm-1.0.2/` - GLM (math library, header-only)
- `assimp-6.0.2/` - Assimp (model loading, built from source)

All libraries are automatically detected and configured.

## Building

### Windows (Visual Studio)

For Windows `.exe` executables:

```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

**Output:** `build/bin/Release/game.exe`

**Note:** First build will take longer (5-15 minutes) because GLFW and Assimp are built from source.

### Docker Build (Linux)

For Linux executables:

```bash
# Build Linux executables
docker-compose run --rm build

# Run the game
docker-compose run --rm run

# Development environment
docker-compose run --rm dev
```

**Output:** `build/bin/game` (Linux executable)

## Running

### Test Window

The current build includes a simple test window that renders a rotating triangle:

```powershell
cd build/bin/Release
./game.exe
```

**Controls:**
- **WASD** - Move camera
- **Mouse** - Look around
- **ESC** - Exit

### Expected Result

- Window opens at 1366x768 resolution
- Dark blue background
- Rotating triangle in center
- Camera controls work

## Libraries

### Integrated Libraries

1. **GLEW 2.1.0** - Pre-built libraries from `game_libs/glew-2.1.0/`
2. **GLFW 3.4** - Built from source in `game_libs/glfw-3.4/`
3. **GLM 1.0.2** - Header-only library in `game_libs/glm-1.0.2/`
4. **Assimp 6.0.2** - Built from source in `game_libs/assimp-6.0.2/`

### Automatic Configuration

The build system automatically:
- Finds all libraries in `game_libs/`
- Builds GLFW and Assimp from source
- Copies DLLs to output directory
- Links everything together

## Rendering System

### Current Capabilities

- ✅ OpenGL window management (GLFW)
- ✅ Mesh rendering (VAO/VBO/IBO)
- ✅ Shader compilation and management
- ✅ Texture loading (stb_image)
- ✅ Material system
- ✅ Camera system
- ✅ Model loading (.obj, .fbx via Assimp)

### Shader System

Shaders are located in `engine/platform/render/shaders/`:
- `shader.vert` - Vertex shader
- `shader.frag` - Fragment shader with lighting support

Shaders are automatically copied to build directory during configuration.

### Model Loading

Models can be loaded using the Model class:

```cpp
#include "render/Model.h"

using namespace Engine::Platform::Render;

Model characterModel;
characterModel.LoadModel("graphics/Models/character/graphics/male_model.obj");
characterModel.RenderModel();
```

**Supported formats:** `.obj`, `.fbx`, `.dae`, `.gltf`, and many more via Assimp.

## Troubleshooting

### CMake Version Error

If you see an error about CMake 3.22:
- Assimp 6.0.2 requires CMake 3.22+
- Download from: https://cmake.org/download/

### Libraries Not Found

The build system looks for libraries in `../game_libs/` relative to the project root:
```
games/
├── thegame/     (project)
└── game_libs/      (libraries)
```

Ensure this structure exists.

### Long First Build

The first build takes longer because:
- GLFW is built from source (~2-3 minutes)
- Assimp is built from source (~5-15 minutes)
- This is a one-time cost - subsequent builds are incremental

### Shader Files Not Found

Shaders are automatically copied during CMake configuration. If missing:
- Check `build/bin/engine/platform/render/shaders/`
- Or manually copy from `engine/platform/render/shaders/`

### OpenGL Errors

- Ensure graphics drivers are up to date
- Check that OpenGL 3.3+ is supported
- Verify GLEW initialization succeeded

## Development

### Project Components

- **Engine** (`engine/core/`) - Core simulation engine with ECS, fixed timestep, job system
- **Platform** (`engine/platform/`) - Rendering, audio, input, network layer
- **Game** (`src/`) - Final game executable

### Adding New Features

See `PROJECT_PROGRESS.md` for current status and next steps.

## Notes

- The Platform library is a STATIC library
- All rendering classes use the `Engine::Platform::Render` namespace
- First build of GLFW/Assimp is slower, but subsequent builds are fast
- DLLs are automatically copied to output directory
