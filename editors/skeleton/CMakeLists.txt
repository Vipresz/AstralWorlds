cmake_minimum_required(VERSION 3.22)

# Skeleton Editor Executable
project(SkeletonEditor)

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
    message(STATUS "Skeleton Editor: Using Homebrew prefix: ${BREW_PREFIX}")
else()
    set(USE_HOMEBREW FALSE)
endif()

set(GAME_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../game_libs")

# Find Nuklear directory (check multiple locations)
set(NUKLEAR_DIR "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../Nuklear/nuklear.h")
    set(NUKLEAR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../Nuklear")
elseif(EXISTS "${GAME_LIBS_DIR}/Nuklear/nuklear.h")
    set(NUKLEAR_DIR "${GAME_LIBS_DIR}/Nuklear")
endif()

if(NUKLEAR_DIR)
    message(STATUS "Found Nuklear at: ${NUKLEAR_DIR}")
else()
    message(WARNING "Nuklear not found - skeleton editor may not build")
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
    # Try find_package as last resort
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
        ${CMAKE_CURRENT_SOURCE_DIR}/../../engine
        ${CMAKE_CURRENT_SOURCE_DIR}/../../engine/platform
    )
else()
include_directories(
        ${GAME_LIBS_DIR}/glm-1.0.2
        ${NUKLEAR_DIR}
        ${NUKLEAR_DIR}/demo/glfw_opengl3
    ${CMAKE_CURRENT_SOURCE_DIR}/../../engine
    ${CMAKE_CURRENT_SOURCE_DIR}/../../engine/platform
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
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../build/lib/Release
)
endif()

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
    LibIGLWrapper.cpp
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

# Link libraries for command-line editor (if needed)
# Note: skeleton_editor doesn't use OpenGL, so GLEW not needed

# Link libraries for visual editor
if(GLEW_LIB)
    target_link_libraries(visual_skeleton_editor ${GLEW_LIB})
    # Copy GLEW DLL to executable directory (Windows only)
    if(NOT USE_HOMEBREW)
        set(GLEW_DLL_DIR "${GAME_LIBS_DIR}/glew-2.1.0/bin/Release/x64")
    if(EXISTS "${GLEW_DLL_DIR}/glew32.dll")
        add_custom_command(TARGET visual_skeleton_editor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GLEW_DLL_DIR}/glew32.dll"
            "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
            COMMENT "Copying GLEW DLL to visual_skeleton_editor output directory"
        )
        endif()
    endif()
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

# macOS-specific frameworks and libraries
if(APPLE)
    find_package(OpenGL REQUIRED)
    target_link_libraries(visual_skeleton_editor OpenGL::GL)
    
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(IOKIT_FRAMEWORK IOKit)
    find_library(COREVIDEO_FRAMEWORK CoreVideo)
    if(COCOA_FRAMEWORK AND IOKIT_FRAMEWORK AND COREVIDEO_FRAMEWORK)
        target_link_libraries(visual_skeleton_editor 
            ${COCOA_FRAMEWORK} 
            ${IOKIT_FRAMEWORK} 
            ${COREVIDEO_FRAMEWORK}
        )
    endif()
endif()

# libigl integration for advanced skeleton and weight extraction
set(LIBIGL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../game_libs/libigl")
set(LIBIGL_INCLUDE_DIR "${LIBIGL_DIR}/include")

# Check if libigl exists
if(EXISTS "${LIBIGL_INCLUDE_DIR}/igl/heat_geodesics.h")
    message(STATUS "Found libigl at: ${LIBIGL_DIR}")
    
    # Add libigl include directory
    include_directories(${LIBIGL_INCLUDE_DIR})
    target_include_directories(visual_skeleton_editor PRIVATE ${LIBIGL_INCLUDE_DIR})
    
    # libigl requires Eigen - try to find it
    # Check game_libs/eigen first (user's location)
    set(EIGEN_FOUND FALSE)
    set(GAME_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../game_libs")
    set(EIGEN_DIRS
        "${GAME_LIBS_DIR}/eigen"           # Primary location (user specified)
        "${GAME_LIBS_DIR}/eigen3"
        "${GAME_LIBS_DIR}/Eigen"
        "${GAME_LIBS_DIR}/Eigen3"
        "${LIBIGL_DIR}/external/eigen"
    )
    
    foreach(EIGEN_DIR ${EIGEN_DIRS})
        if(EXISTS "${EIGEN_DIR}/Eigen/Eigen" OR EXISTS "${EIGEN_DIR}/Eigen/Dense")
            message(STATUS "Found Eigen at: ${EIGEN_DIR}")
            include_directories(${EIGEN_DIR})
            target_include_directories(visual_skeleton_editor PRIVATE ${EIGEN_DIR})
            set(EIGEN_FOUND TRUE)
            break()
        endif()
    endforeach()
    
    # If not found in common locations, try find_package
    if(NOT EIGEN_FOUND)
        find_package(Eigen3 QUIET)
        if(Eigen3_FOUND)
            message(STATUS "Found Eigen3 via find_package")
            target_link_libraries(visual_skeleton_editor PRIVATE Eigen3::Eigen)
            set(EIGEN_FOUND TRUE)
        endif()
    endif()
    
    # Only enable libigl if Eigen is found
    if(EIGEN_FOUND)
        # Define LIBIGL_AVAILABLE so the code knows libigl is available
        target_compile_definitions(visual_skeleton_editor PRIVATE LIBIGL_AVAILABLE)
        message(STATUS "libigl integration enabled with Eigen")
    else()
        message(WARNING "Eigen not found")
        message(WARNING "libigl requires Eigen")
        message(WARNING "Searched in game_libs/eigen and other common locations")
        message(WARNING "libigl features will be disabled until Eigen is found")
    endif()
else()
    message(STATUS "libigl not found at ${LIBIGL_DIR} - using fallback methods")
    message(STATUS "Code will work without libigl using distance-based weight generation")
endif()

# Compiler options
if(MSVC)
    target_compile_options(skeleton_editor PRIVATE /W4 /permissive- /Zc:__cplusplus /EHsc)
    target_compile_definitions(skeleton_editor PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    target_compile_options(visual_skeleton_editor PRIVATE /W4 /permissive- /Zc:__cplusplus /EHsc)
    target_compile_definitions(visual_skeleton_editor PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    target_compile_options(skeleton_editor PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
    target_compile_options(visual_skeleton_editor PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# C++ standard
set_target_properties(skeleton_editor PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

message(STATUS "Skeleton Editor configured")

