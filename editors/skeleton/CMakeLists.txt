cmake_minimum_required(VERSION 3.22)

# Skeleton Editor Executable
project(SkeletonEditor)

# Find GLM (required for math types)
find_package(glm QUIET)
if(NOT glm_FOUND)
    # GLM is header-only, so we can just include the directory
    set(GLM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../game_libs/glm-1.0.2")
    if(EXISTS "${GLM_DIR}/glm/glm.hpp")
        message(STATUS "Using GLM from: ${GLM_DIR}")
    else()
        message(FATAL_ERROR "GLM not found. Please ensure game_libs/glm-1.0.2 exists.")
    endif()
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../game_libs/glm-1.0.2
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../game_libs/Nuklear
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../game_libs/Nuklear/demo/glfw_opengl3
    ${CMAKE_CURRENT_SOURCE_DIR}/../../engine
    ${CMAKE_CURRENT_SOURCE_DIR}/../../engine/platform
)

# Find GLEW and GLFW (should be in game_libs)
find_library(GLEW_LIB 
    NAMES glew32 glew
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../../game_libs/glew-2.1.0/lib/Release
)

find_library(GLFW_LIB
    NAMES glfw3 glfw
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../../game_libs/glfw-3.4
)

# Find Assimp
find_library(ASSIMP_LIB
    NAMES assimp-vc143-mt assimp
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../build/lib/Release
)

# Source files for command-line editor
set(SOURCES
    SkeletonDefinition.cpp
    SkeletonSerializer.cpp
    SkeletonEditor.cpp
    main.cpp
)

# Source files for visual editor
set(VISUAL_SOURCES
    SkeletonDefinition.cpp
    SkeletonSerializer.cpp
    SkeletonEditor.cpp
    VisualSkeletonEditor.cpp
    Animation.cpp
    NuklearImpl.cpp
    ConsoleRedirect.cpp
    visual_main.cpp
)

# Create command-line executable
add_executable(skeleton_editor ${SOURCES})

# Create visual editor executable
add_executable(visual_skeleton_editor ${VISUAL_SOURCES})

# Set output directory
set_target_properties(skeleton_editor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

set_target_properties(visual_skeleton_editor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Link libraries for visual editor
if(GLEW_LIB)
    target_link_libraries(visual_skeleton_editor ${GLEW_LIB})
endif()

if(GLFW_LIB)
    target_link_libraries(visual_skeleton_editor ${GLFW_LIB})
endif()

if(ASSIMP_LIB)
    target_link_libraries(visual_skeleton_editor ${ASSIMP_LIB})
endif()

# Link platform library if it exists
if(TARGET Platform)
    target_link_libraries(visual_skeleton_editor Platform)
endif()

# Compiler options
if(MSVC)
    target_compile_options(skeleton_editor PRIVATE /W4 /permissive- /Zc:__cplusplus /EHsc)
    target_compile_definitions(skeleton_editor PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(skeleton_editor PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# C++ standard
set_target_properties(skeleton_editor PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

message(STATUS "Skeleton Editor configured")

