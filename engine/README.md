# Game Engine

Core simulation engine library. Provides ECS, fixed timestep, job system, and platform abstraction.

## Engine Structure

```
engine/
├── core/              # Core engine systems
│   ├── ecs/          # Entity Component System
│   ├── math/         # Deterministic math library
│   ├── jobs/         # Multithreaded job system
│   └── time/         # Fixed timestep management
├── platform/         # Platform/render layer (rendering, audio, input, network)
└── examples/         # Example code
```

**Note:** Game-specific systems (inventory, crafting, AI, economy) are located in `src/systems/` as they are specific to the game being developed, not part of the reusable engine.

## Building

The engine is built as part of the main project. See the [root README](../README.md) for build instructions.

### Build Engine Only (for testing)

**Windows:**
```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

**Linux/macOS:**
```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## Usage

See `examples/BasicExample.cpp` for a complete example demonstrating the ECS system, fixed timestep, and entity updates.

The engine is linked into the game executable via CMake. Include headers like:
```cpp
#include "core/ecs/Registry.h"
#include "core/System.h"
#include "core/time/GameLoop.h"
```
