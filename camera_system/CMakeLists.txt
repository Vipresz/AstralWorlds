cmake_minimum_required(VERSION 3.22)

# Camera System Editor Executable
project(CameraSystemEditor)

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
    message(STATUS "Camera System Editor: Using Homebrew prefix: ${BREW_PREFIX}")
else()
    set(USE_HOMEBREW FALSE)
endif()

set(GAME_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../game_libs")

# Find Nuklear directory (check multiple locations)
set(NUKLEAR_DIR "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../Nuklear/nuklear.h")
    set(NUKLEAR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../Nuklear")
elseif(EXISTS "${GAME_LIBS_DIR}/Nuklear/nuklear.h")
    set(NUKLEAR_DIR "${GAME_LIBS_DIR}/Nuklear")
endif()

if(NUKLEAR_DIR)
    message(STATUS "Found Nuklear at: ${NUKLEAR_DIR}")
else()
    message(WARNING "Nuklear not found - camera system editor may not build")
endif()

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
        ${NUKLEAR_DIR}
        ${NUKLEAR_DIR}/demo/glfw_opengl3
        ${CMAKE_CURRENT_SOURCE_DIR}/../engine
        ${CMAKE_CURRENT_SOURCE_DIR}/../engine/platform
    )
else()
    include_directories(
        ${GAME_LIBS_DIR}/glm-1.0.2
        ${NUKLEAR_DIR}
        ${NUKLEAR_DIR}/demo/glfw_opengl3
        ${CMAKE_CURRENT_SOURCE_DIR}/../engine
        ${CMAKE_CURRENT_SOURCE_DIR}/../engine/platform
    )
endif()

# ============================================================================
# Find GLEW, GLFW, and Assimp
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
    
    find_library(ASSIMP_LIB
        NAMES assimp
        PATHS ${BREW_PREFIX}/opt/assimp/lib
        NO_DEFAULT_PATH
    )
    
    # Add include paths for Homebrew libraries
    include_directories(
        ${BREW_PREFIX}/opt/glew/include
        ${BREW_PREFIX}/opt/glfw/include
        ${BREW_PREFIX}/opt/assimp/include
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

    find_library(ASSIMP_LIB
        NAMES assimp-vc143-mt assimp
        PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../build/lib/Release
    )
endif()

# Source files for camera system editor
set(SOURCES
    animation/Animation.cpp
    animation/Animation.h
    camera/CameraSystem.cpp
    camera/CameraSystem.h
    logs/ConsoleRedirect.cpp
    logs/ConsoleRedirect.h
    helper/ConfigLoader.cpp
    helper/ConfigLoader.h
    config/ConfigManager.cpp
    config/ConfigManager.h
    config/ConfigManager.inl
    config/UIConfig.cpp
    config/UIConfig.h
    nuklear/NuklearUI.cpp
    nuklear/NuklearUI.h
    nuklear/NuklearImpl.cpp
    CameraSystemEditor.cpp
    CameraSystemEditor.h
    GlobalConstants.h
    remesh/Remeshing.cpp
    remesh/Remeshing.h
    tof/TOFProcessing.cpp
    tof/TOFProcessing.h
    occupancy/OccupancyAlgorithms.cpp
    occupancy/OccupancyAlgorithms.h
    algorithms/LowPassFilter.cpp
    algorithms/LowPassFilter.h
    world/World.cpp
    world/World.h
    ui/CameraSystemUI.cpp
    ui/CameraSystemUI.h
    ui/UIHelpers.cpp
    ui/UIHelpers.h
    ui/Plotting.cpp
    ui/Plotting.h
    objects/ObjectManager.cpp
    objects/ObjectManager.h
    objects/ObjectRenderer.cpp
    objects/ObjectRenderer.h
    main.cpp
)

# Create executable
add_executable(camera_system_editor ${SOURCES})

# Set output directory
set_target_properties(camera_system_editor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Link libraries
if(GLEW_LIB)
    target_link_libraries(camera_system_editor ${GLEW_LIB})
    # Copy GLEW DLL to executable directory (Windows only)
    if(NOT USE_HOMEBREW)
        set(GLEW_DLL_DIR "${GAME_LIBS_DIR}/glew-2.1.0/bin/Release/x64")
        if(EXISTS "${GLEW_DLL_DIR}/glew32.dll")
            add_custom_command(TARGET camera_system_editor POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${GLEW_DLL_DIR}/glew32.dll"
                "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
                COMMENT "Copying GLEW DLL to camera_system_editor output directory"
            )
        endif()
    endif()
endif()

if(GLFW_LIB)
    target_link_libraries(camera_system_editor ${GLFW_LIB})
endif()

if(ASSIMP_LIB)
    target_link_libraries(camera_system_editor ${ASSIMP_LIB})
endif()

# Link platform library if it exists
if(TARGET Platform)
    target_link_libraries(camera_system_editor Platform)
endif()

# macOS-specific frameworks and libraries
if(APPLE)
    find_package(OpenGL REQUIRED)
    target_link_libraries(camera_system_editor OpenGL::GL)
    
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(IOKIT_FRAMEWORK IOKit)
    find_library(COREVIDEO_FRAMEWORK CoreVideo)
    if(COCOA_FRAMEWORK AND IOKIT_FRAMEWORK AND COREVIDEO_FRAMEWORK)
        target_link_libraries(camera_system_editor 
            ${COCOA_FRAMEWORK} 
            ${IOKIT_FRAMEWORK} 
            ${COREVIDEO_FRAMEWORK}
        )
    endif()
endif()

# Compiler options
if(MSVC)
    target_compile_options(camera_system_editor PRIVATE /W4 /permissive- /Zc:__cplusplus /EHsc)
    target_compile_definitions(camera_system_editor PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(camera_system_editor PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# C++ standard
set_target_properties(camera_system_editor PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

message(STATUS "Camera System Editor configured")

