cmake_minimum_required(VERSION 3.22)

# Enable OBJCXX on macOS for Objective-C++ support (needed for Gear 360 Bluetooth)
if(APPLE)
    project(GameEngine VERSION 0.1.1 LANGUAGES CXX OBJCXX)
else()
    project(GameEngine VERSION 0.1.1 LANGUAGES CXX)
endif()

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler options
if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus)
    add_compile_options(/EHsc)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/MP)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_compile_options(-Wno-unused-parameter)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Nuklear (menus / 2D overlay) — prefer bundled copy in RenderingEngine
set(RENDERING_ENGINE_NUKLEAR "${CMAKE_CURRENT_SOURCE_DIR}/RenderingEngine/third_party/nuklear")
set(EXTERNAL_NUKLEAR "${CMAKE_CURRENT_SOURCE_DIR}/external/Nuklear")
get_filename_component(GAMES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(GAME_LIBS_DIR "${GAMES_DIR}/game_libs")

if(EXISTS "${RENDERING_ENGINE_NUKLEAR}/nuklear.h")
    set(NUKLEAR_DIR "${RENDERING_ENGINE_NUKLEAR}" CACHE PATH "Nuklear library root")
    message(STATUS "Nuklear found (bundled): ${NUKLEAR_DIR}")
elseif(EXISTS "${EXTERNAL_NUKLEAR}/nuklear.h")
    set(NUKLEAR_DIR "${EXTERNAL_NUKLEAR}" CACHE PATH "Nuklear library root")
    message(STATUS "Nuklear found (external): ${NUKLEAR_DIR}")
elseif(EXISTS "${GAME_LIBS_DIR}/Nuklear/nuklear.h")
    set(NUKLEAR_DIR "${GAME_LIBS_DIR}/Nuklear" CACHE PATH "Nuklear library root")
    message(STATUS "Nuklear found (game_libs): ${NUKLEAR_DIR}")
else()
    message(WARNING "Nuklear not found — UI menus will not build")
endif()

# ============================================================================
# Build Components
# ============================================================================

# Build RenderingEngine library (Engine + Platform for rendering, UI, logging)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RenderingEngine/CMakeLists.txt")
    set(BUILD_ENGINE_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_ENGINE_TESTS ${BUILD_ENGINE_TESTS} CACHE BOOL "Build RenderingEngine unit tests" FORCE)
    if(NOT BUILD_GAME AND NOT BUILD_SKELETON_EDITOR AND BUILD_ENGINE_TESTS AND NOT BUILD_GAME_TESTS)
        set(BUILD_ENGINE_PLATFORM OFF CACHE BOOL "" FORCE)
    endif()
    add_subdirectory(RenderingEngine)
    message(STATUS "Added RenderingEngine from: ${CMAKE_CURRENT_SOURCE_DIR}/RenderingEngine")
else()
    message(FATAL_ERROR "RenderingEngine not found at ${CMAKE_CURRENT_SOURCE_DIR}/RenderingEngine")
endif()

# Build options for specific targets
option(BUILD_GAME "Build the main game executable" ON)
option(BUILD_SKELETON_EDITOR "Build the skeleton editor" ON)
option(BUILD_ENGINE_TESTS "Build RenderingEngine unit tests" ON)
option(BUILD_GAME_TESTS "Build AstralWorlds game e2e tests" ON)

if(BUILD_ENGINE_TESTS OR BUILD_GAME_TESTS)
    enable_testing()
endif()

if(BUILD_GAME AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/CMakeLists.txt")
    add_subdirectory(src)
endif()

if(BUILD_GAME_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()

if(BUILD_SKELETON_EDITOR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/editors/skeleton/CMakeLists.txt")
    add_subdirectory(editors/skeleton)
endif()

message(STATUS "")
message(STATUS "Game Engine Build Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build targets:")
message(STATUS "    - Game executable: ${BUILD_GAME}")
message(STATUS "    - Skeleton editor: ${BUILD_SKELETON_EDITOR}")
message(STATUS "")
message(STATUS "To build specific targets, use:")
message(STATUS "  cmake -DBUILD_GAME=OFF -DBUILD_SKELETON_EDITOR=ON ..")
message(STATUS "")
