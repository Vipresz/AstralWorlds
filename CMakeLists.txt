cmake_minimum_required(VERSION 3.22)
project(GameEngine VERSION 0.1.0 LANGUAGES CXX)

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

# Build engine library (core simulation engine)
add_subdirectory(engine)

# Build platform/render library (rendering, audio, input, network)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/engine/platform/CMakeLists.txt")
    add_subdirectory(engine/platform)
endif()

# Build game executable (final player executable)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/CMakeLists.txt")
    add_subdirectory(src)
endif()

message(STATUS "")
message(STATUS "Game Engine Build Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "")
