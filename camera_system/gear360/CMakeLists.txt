cmake_minimum_required(VERSION 3.22)

# Gear 360 Test Application
# Enable OBJCXX for macOS Objective-C++ support (needed for Gear360Stream)
# We enable it unconditionally in project() - it will only be used on macOS
project(Gear360Test LANGUAGES C CXX OBJCXX)

# ============================================================================
# Platform Detection and Dependency Setup
# ============================================================================

if(APPLE)
    # macOS: Use Homebrew paths
    if(DEFINED HOMEBREW_PREFIX)
        set(BREW_PREFIX "${HOMEBREW_PREFIX}")
    elseif(EXISTS "/opt/homebrew")
        set(BREW_PREFIX "/opt/homebrew")  # Apple Silicon
    else()
        set(BREW_PREFIX "/usr/local")  # Intel Mac
    endif()
    set(USE_HOMEBREW TRUE)
    message(STATUS "Gear 360 Test: Using Homebrew prefix: ${BREW_PREFIX}")
else()
    set(USE_HOMEBREW FALSE)
endif()

set(GAME_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../game_libs")

# ============================================================================
# Find GLM (required for math types)
# ============================================================================

set(GLM_FOUND_LOCAL FALSE)

if(USE_HOMEBREW)
    set(GLM_DIR "${BREW_PREFIX}/opt/glm/include")
    if(EXISTS "${GLM_DIR}/glm/glm.hpp")
        message(STATUS "Using GLM (Homebrew): ${GLM_DIR}")
        set(GLM_FOUND_LOCAL TRUE)
    endif()
else()
    set(GLM_DIR "${GAME_LIBS_DIR}/glm-1.0.2")
    if(EXISTS "${GLM_DIR}/glm/glm.hpp")
        message(STATUS "Using GLM from: ${GLM_DIR}")
        set(GLM_FOUND_LOCAL TRUE)
    endif()
endif()

if(NOT GLM_FOUND_LOCAL)
    find_package(glm QUIET)
    if(glm_FOUND)
        set(GLM_FOUND_LOCAL TRUE)
    else()
        message(FATAL_ERROR "GLM not found. On macOS, run: brew install glm")
    endif()
endif()

# ============================================================================
# Include directories
# ============================================================================

if(USE_HOMEBREW)
    include_directories(
        ${BREW_PREFIX}/opt/glm/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../engine
        ${CMAKE_CURRENT_SOURCE_DIR}/../../engine/platform
        ${CMAKE_CURRENT_SOURCE_DIR}/../../engine/platform/render
    )
else()
    include_directories(
        ${GAME_LIBS_DIR}/glm-1.0.2
        ${CMAKE_CURRENT_SOURCE_DIR}/../../engine
        ${CMAKE_CURRENT_SOURCE_DIR}/../../engine/platform
        ${CMAKE_CURRENT_SOURCE_DIR}/../../engine/platform/render
    )
endif()

# ============================================================================
# Find GLEW and GLFW
# ============================================================================

if(USE_HOMEBREW)
    find_library(GLEW_LIB 
        NAMES GLEW glew
        PATHS ${BREW_PREFIX}/opt/glew/lib
        NO_DEFAULT_PATH
    )
    
    find_library(GLFW_LIB
        NAMES glfw
        PATHS ${BREW_PREFIX}/opt/glfw/lib
        NO_DEFAULT_PATH
    )
    
    # Add include paths for Homebrew libraries
    include_directories(
        ${BREW_PREFIX}/opt/glew/include
        ${BREW_PREFIX}/opt/glfw/include
    )
else()
    find_library(GLEW_LIB 
        NAMES glew32 glew
        PATHS ${GAME_LIBS_DIR}/glew-2.1.0/lib/Release
    )

    find_library(GLFW_LIB
        NAMES glfw3 glfw
        PATHS ${GAME_LIBS_DIR}/glfw-3.4
    )
endif()

# ============================================================================
# Source files
# ============================================================================

set(GEAR360_SOURCES
    Gear360Stream.cpp
    Gear360Stream.h
    gear360_test.cpp
)

# ============================================================================
# Create executable
# ============================================================================

add_executable(gear360_test ${GEAR360_SOURCES})

# Set Objective-C++ compilation for Gear360Stream on macOS
if(APPLE)
    set_source_files_properties(Gear360Stream.cpp PROPERTIES
        LANGUAGE OBJCXX
    )
endif()

# Set output directory
set_target_properties(gear360_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ============================================================================
# Link libraries
# ============================================================================

if(GLEW_LIB)
    target_link_libraries(gear360_test ${GLEW_LIB})
endif()

if(GLFW_LIB)
    target_link_libraries(gear360_test ${GLFW_LIB})
endif()

# macOS-specific frameworks and libraries
if(APPLE)
    find_package(OpenGL REQUIRED)
    target_link_libraries(gear360_test OpenGL::GL)
    
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(IOKIT_FRAMEWORK IOKit)
    find_library(COREVIDEO_FRAMEWORK CoreVideo)
    find_library(COREBLUETOOTH_FRAMEWORK CoreBluetooth)
    find_library(FOUNDATION_FRAMEWORK Foundation)
    if(COCOA_FRAMEWORK AND IOKIT_FRAMEWORK AND COREVIDEO_FRAMEWORK)
        target_link_libraries(gear360_test 
            ${COCOA_FRAMEWORK} 
            ${IOKIT_FRAMEWORK} 
            ${COREVIDEO_FRAMEWORK}
        )
    endif()
    if(COREBLUETOOTH_FRAMEWORK AND FOUNDATION_FRAMEWORK)
        target_link_libraries(gear360_test 
            ${COREBLUETOOTH_FRAMEWORK}
            ${FOUNDATION_FRAMEWORK}
        )
    endif()
endif()

# ============================================================================
# Compiler options
# ============================================================================

if(MSVC)
    target_compile_options(gear360_test PRIVATE /W4 /permissive- /Zc:__cplusplus /EHsc)
    target_compile_definitions(gear360_test PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(gear360_test PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# C++ standard
set_target_properties(gear360_test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

message(STATUS "Gear 360 Test application configured")
message(STATUS "  - GLEW: ${GLEW_LIB}")
message(STATUS "  - GLFW: ${GLFW_LIB}")
message(STATUS "  - GLM: ${GLM_DIR}")
