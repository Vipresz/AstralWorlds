cmake_minimum_required(VERSION 3.22)

# Camera System Editor Executable
project(CameraSystemEditor)

# Find GLM (required for math types)
find_package(glm QUIET)
if(NOT glm_FOUND)
    # GLM is header-only, so we can just include the directory
    # camera_system is at thegame/camera_system/, so we need to go up 2 levels to reach game_libs
    set(GLM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../game_libs/glm-1.0.2")
    if(EXISTS "${GLM_DIR}/glm/glm.hpp")
        message(STATUS "Using GLM from: ${GLM_DIR}")
    else()
        message(FATAL_ERROR "GLM not found. Please ensure game_libs/glm-1.0.2 exists.")
    endif()
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../game_libs/glm-1.0.2
    ${CMAKE_CURRENT_SOURCE_DIR}/../../game_libs/Nuklear
    ${CMAKE_CURRENT_SOURCE_DIR}/../../game_libs/Nuklear/demo/glfw_opengl3
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/platform
)

# Find GLEW and GLFW (should be in game_libs)
find_library(GLEW_LIB 
    NAMES glew32 glew
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../game_libs/glew-2.1.0/lib/Release
)

find_library(GLFW_LIB
    NAMES glfw3 glfw
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../game_libs/glfw-3.4
)

# Find Assimp
find_library(ASSIMP_LIB
    NAMES assimp-vc143-mt assimp
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../build/lib/Release
)

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
    world/World.cpp
    world/World.h
    ui/CameraSystemUI.cpp
    ui/CameraSystemUI.h
    ui/UIHelpers.cpp
    ui/UIHelpers.h
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
    # Copy GLEW DLL to executable directory
    set(GLEW_DLL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../game_libs/glew-2.1.0/bin/Release/x64")
    if(EXISTS "${GLEW_DLL_DIR}/glew32.dll")
        add_custom_command(TARGET camera_system_editor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GLEW_DLL_DIR}/glew32.dll"
            "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
            COMMENT "Copying GLEW DLL to camera_system_editor output directory"
        )
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

