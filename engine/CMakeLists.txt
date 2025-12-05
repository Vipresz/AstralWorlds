# Engine Library - Core simulation engine (ECS, timestep, jobs, etc.)

# ============================================================================
# Engine Library (Header-only INTERFACE library for now)
# ============================================================================

# Create INTERFACE library (since engine is currently header-only)
# When .cpp files are added later, we can convert this to a STATIC library
add_library(Engine INTERFACE)

# Include directories - point to engine root so includes work like:
# #include "core/ecs/Registry.h"
target_include_directories(Engine INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# C++ standard
target_compile_features(Engine INTERFACE cxx_std_17)

# Platform-specific libraries
find_package(Threads REQUIRED)
target_link_libraries(Engine INTERFACE Threads::Threads)

# Windows-specific settings
if(WIN32)
    # Windows needs this for Sleep() and other functions
    target_link_libraries(Engine INTERFACE winmm)
endif()

# ============================================================================
# Example/Simulation Executables
# ============================================================================

# Basic Example Simulation
add_executable(BasicSimulation
    examples/BasicExample.cpp
)

target_link_libraries(BasicSimulation PRIVATE Engine)

# Set output name - CMake automatically adds .exe on Windows
set_target_properties(BasicSimulation PROPERTIES
    OUTPUT_NAME "basic_simulation"
)

# Optional: Set working directory for running from IDE
set_target_properties(BasicSimulation PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)


# ============================================================================
# Future: Add source files here when creating .cpp implementations
# ============================================================================

# Example for when you add .cpp files:
# set(ENGINE_CORE_SOURCES
#     core/time/GameLoop.cpp
#     core/jobs/JobSystem.cpp
# )
# 
# # Convert to STATIC library
# set_target_properties(Engine PROPERTIES TYPE STATIC)
# target_sources(Engine PRIVATE ${ENGINE_CORE_SOURCES})

# ============================================================================
# Third-party dependencies (commented out for future use)
# ============================================================================

# OpenGL support (uncomment when needed)
# find_package(OpenGL REQUIRED)
# target_link_libraries(Engine INTERFACE OpenGL::GL)

# JSON library (example: nlohmann/json)
# add_subdirectory(third_party/json)
