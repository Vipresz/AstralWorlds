cmake_minimum_required(VERSION 3.22)

# Enable OBJCXX on macOS for Objective-C++ support (needed for Gear 360 Bluetooth)
if(APPLE)
    project(GameEngine VERSION 0.1.0 LANGUAGES CXX OBJCXX)
else()
    project(GameEngine VERSION 0.1.0 LANGUAGES CXX)
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

# ============================================================================
# Build Components
# ============================================================================

# Build RenderingEngine library (Engine + Platform for rendering, UI, logging)
# This provides generic components that can be reused across projects
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RenderingEngine/CMakeLists.txt")
    set(BUILD_ENGINE_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(RenderingEngine)
    message(STATUS "Added RenderingEngine from: ${CMAKE_CURRENT_SOURCE_DIR}/RenderingEngine")
else()
    message(FATAL_ERROR "RenderingEngine not found at ${CMAKE_CURRENT_SOURCE_DIR}/RenderingEngine")
endif()

# Build options for specific targets
option(BUILD_GAME "Build the main game executable" ON)
option(BUILD_SKELETON_EDITOR "Build the skeleton editor" ON)

# Build game executable (final player executable)
if(BUILD_GAME AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/CMakeLists.txt")
    add_subdirectory(src)
endif()

# Build skeleton editor
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
