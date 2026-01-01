# Platform/Render Library - Rendering, audio, input, network

# ============================================================================
# Platform/Render Library
# ============================================================================

# Platform source files
set(PLATFORM_SOURCES
    render/Window.cpp
    render/Mesh.cpp
    render/Shader.cpp
    render/Texture.cpp
    render/Material.cpp
    render/Camera.cpp
    render/FollowCamera.cpp
    render/Model.cpp
    render/Animation.cpp
    render/Skeleton.cpp
    render/systems/RenderSystem.cpp
    render/systems/AnimationSystem.cpp
    input/InputSystem.cpp
)

# Create STATIC library for platform layer (since we have .cpp files)
add_library(Platform STATIC ${PLATFORM_SOURCES})

target_include_directories(Platform PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>  # For engine headers
)

target_compile_features(Platform PUBLIC cxx_std_17)

# Link to engine (platform layer uses engine)
target_link_libraries(Platform PUBLIC Engine)

# ============================================================================
# Rendering Dependencies - Using game_libs directory
# ============================================================================

# Set base path to game_libs directory
# From engine/platform/CMakeLists.txt:
#   Up 2 levels = Astrumoria/
#   Up 1 more = games/
#   Then into game_libs/
get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(GAMES_DIR "${PROJECT_ROOT}/.." ABSOLUTE)
set(GAME_LIBS_DIR "${GAMES_DIR}/game_libs")

message(STATUS "Using game_libs directory: ${GAME_LIBS_DIR}")

# ============================================================================
# Copy required header files
# ============================================================================

# Copy stb_image.h from graphics/openGL_example to render directory if it doesn't exist
set(STB_IMAGE_SOURCE "${PROJECT_ROOT}/graphics/openGL_example/stb_image.h")
set(STB_IMAGE_DEST "${CMAKE_CURRENT_SOURCE_DIR}/render/stb_image.h")

if(EXISTS "${STB_IMAGE_SOURCE}" AND NOT EXISTS "${STB_IMAGE_DEST}")
    file(COPY "${STB_IMAGE_SOURCE}" DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/render/")
    message(STATUS "Copied stb_image.h to render directory")
elseif(NOT EXISTS "${STB_IMAGE_SOURCE}")
    message(WARNING "stb_image.h not found at ${STB_IMAGE_SOURCE}")
    message(WARNING "  Texture loading will not work without this file")
elseif(EXISTS "${STB_IMAGE_DEST}")
    message(STATUS "stb_image.h already exists in render directory")
endif()

# OpenGL support
find_package(OpenGL REQUIRED)
target_link_libraries(Platform PUBLIC OpenGL::GL)

# GLEW for OpenGL extensions
set(GLEW_ROOT "${GAME_LIBS_DIR}/glew-2.1.0")
set(GLEW_INCLUDE_DIR "${GLEW_ROOT}/include")
set(GLEW_LIB_DIR "${GLEW_ROOT}/lib/Release/x64")
set(GLEW_DLL_DIR "${GLEW_ROOT}/bin/Release/x64")

if(EXISTS "${GLEW_INCLUDE_DIR}/GL/glew.h" AND EXISTS "${GLEW_LIB_DIR}/glew32.lib")
    target_include_directories(Platform PUBLIC ${GLEW_INCLUDE_DIR})
    target_link_libraries(Platform PUBLIC "${GLEW_LIB_DIR}/glew32.lib")
    message(STATUS "Found GLEW: ${GLEW_ROOT}")
    
    # Copy DLL to output directory
    if(EXISTS "${GLEW_DLL_DIR}/glew32.dll")
        # Copy to Platform library output directory
        add_custom_command(TARGET Platform POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GLEW_DLL_DIR}/glew32.dll"
            "$<TARGET_FILE_DIR:Platform>"
            COMMENT "Copying GLEW DLL to output directory"
        )
    endif()
else()
    message(WARNING "GLEW not found at ${GLEW_ROOT}")
endif()

# GLFW for windowing
set(GLFW_ROOT "${GAME_LIBS_DIR}/glfw-3.4")
set(GLFW_INCLUDE_DIR "${GLFW_ROOT}/include")

if(EXISTS "${GLFW_INCLUDE_DIR}/GLFW/glfw3.h")
    # GLFW is source-only, add it as a subdirectory and build it
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    
    add_subdirectory("${GLFW_ROOT}" "${CMAKE_BINARY_DIR}/glfw" EXCLUDE_FROM_ALL)
    target_include_directories(glfw INTERFACE ${GLFW_INCLUDE_DIR})
    target_link_libraries(Platform PUBLIC glfw)
    message(STATUS "Found GLFW (building from source): ${GLFW_ROOT}")
else()
    message(WARNING "GLFW not found at ${GLFW_ROOT}")
endif()

# GLM for math (header-only)
set(GLM_ROOT "${GAME_LIBS_DIR}/glm-1.0.2")
set(GLM_INCLUDE_DIR "${GLM_ROOT}")

if(EXISTS "${GLM_INCLUDE_DIR}/glm/glm.hpp")
    target_include_directories(Platform PUBLIC ${GLM_INCLUDE_DIR})
    message(STATUS "Found GLM: ${GLM_ROOT}")
else()
    message(WARNING "GLM not found at ${GLM_ROOT}")
endif()

# Assimp for model loading - Try new 6.0.2 location first
set(ASSIMP_NEW_ROOT "${GAME_LIBS_DIR}/assimp-6.0.2")
set(ASSIMP_OLD_ROOT "${GAME_LIBS_DIR}/assimp")
set(ASSIMP_LIB_DIR "${GAME_LIBS_DIR}/assimp_lib")  # New pre-built library location
set(ASSIMP_INCLUDE_DIR "${ASSIMP_NEW_ROOT}/include")

# Check if we have complete Assimp 6.0.2 source code
# The source needs CMake modules to build properly
set(ASSIMP_CMAKE_MODULES_DIR "${ASSIMP_NEW_ROOT}/cmake-modules")
set(ASSIMP_HAS_COMPLETE_SOURCE FALSE)

if(EXISTS "${ASSIMP_INCLUDE_DIR}/assimp/Importer.hpp" 
   AND EXISTS "${ASSIMP_NEW_ROOT}/CMakeLists.txt"
   AND EXISTS "${ASSIMP_NEW_ROOT}/contrib/zlib")
    # Check if critical CMake modules exist
    if(EXISTS "${ASSIMP_CMAKE_MODULES_DIR}/assimp-plain-config.cmake.in")
        set(ASSIMP_HAS_COMPLETE_SOURCE TRUE)
    endif()
endif()

# Try to build from source if we have complete source code
if(ASSIMP_HAS_COMPLETE_SOURCE)
    # Assimp 6.0.2 requires CMake 3.22+ (already checked at root level)
    # Build Assimp from source as a subdirectory (like GLFW)
    
    # Configure Assimp build options - disable unnecessary components
    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)  # Build zlib from source (bundled in contrib)
    
    # Add Assimp as a subdirectory - it will be built automatically
    add_subdirectory("${ASSIMP_NEW_ROOT}" "${CMAKE_BINARY_DIR}/assimp" EXCLUDE_FROM_ALL)
    
    # Link to the assimp target created by Assimp's CMakeLists.txt
    target_link_libraries(Platform PUBLIC assimp)
    target_include_directories(Platform PUBLIC ${ASSIMP_INCLUDE_DIR})
    
    message(STATUS "Found Assimp 6.0.2 source: ${ASSIMP_NEW_ROOT}")
    message(STATUS "  Building Assimp from source...")
    message(STATUS "  Building ZLIB from bundled source...")
    message(STATUS "  This may take a few minutes on first build")
    
    # DLL will be built automatically - copy it after build
    add_custom_command(TARGET Platform POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Assimp library built and linked"
        COMMENT "Assimp library built and linked"
    )
else()
    # Source is incomplete, try to find pre-built libraries
    message(STATUS "Assimp 6.0.2 source found but incomplete (missing CMake modules)")
    message(STATUS "  Attempting to use pre-built libraries instead...")
    
    # Fallback: Try to find pre-built Assimp libraries
    find_path(ASSIMP_INCLUDE_DIR_FOUND 
        NAMES assimp/Importer.hpp
        PATHS
            "${ASSIMP_NEW_ROOT}/include"
            "${ASSIMP_OLD_ROOT}/include"
            "${GAME_LIBS_DIR}/assimp/include"
        PATH_SUFFIXES include
        NO_DEFAULT_PATH
    )
    
    find_library(ASSIMP_LIBRARY
        NAMES assimp assimp-vc143-mt assimp-vc142-mt
        PATHS
            "${ASSIMP_LIB_DIR}"
            "${ASSIMP_NEW_ROOT}/lib"
            "${ASSIMP_OLD_ROOT}/lib"
            "${GAME_LIBS_DIR}/assimp/lib"
        PATH_SUFFIXES lib lib/Release lib/x64 Release/lib Debug/lib
        NO_DEFAULT_PATH
    )
    
    # Look for DLL - check new location first
    set(ASSIMP_DLL "${ASSIMP_LIB_DIR}/assimp-vc143-mt.dll")
    if(NOT EXISTS "${ASSIMP_DLL}")
        set(ASSIMP_DLL "${ASSIMP_OLD_ROOT}/assimp-vc143-mt.dll")
    endif()
    if(NOT EXISTS "${ASSIMP_DLL}")
        file(GLOB_RECURSE ASSIMP_DLL_FOUND 
            "${ASSIMP_LIB_DIR}/*.dll"
            "${GAME_LIBS_DIR}/*assimp*.dll"
        )
        if(ASSIMP_DLL_FOUND)
            list(GET ASSIMP_DLL_FOUND 0 ASSIMP_DLL)
        endif()
    endif()
    
    if(ASSIMP_INCLUDE_DIR_FOUND AND ASSIMP_LIBRARY)
        target_include_directories(Platform PUBLIC ${ASSIMP_INCLUDE_DIR_FOUND})
        target_link_libraries(Platform PUBLIC ${ASSIMP_LIBRARY})
        message(STATUS "Found Assimp (pre-built): ${ASSIMP_INCLUDE_DIR_FOUND}")
        message(STATUS "  Library: ${ASSIMP_LIBRARY}")
        
        if(EXISTS "${ASSIMP_DLL}")
            add_custom_command(TARGET Platform POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ASSIMP_DLL}"
                $<TARGET_FILE_DIR:Game>
            )
            message(STATUS "  DLL: ${ASSIMP_DLL}")
        endif()
    else()
        message(WARNING "Assimp source is incomplete (missing CMake modules) and no pre-built libraries found")
        message(WARNING "  Source location: ${ASSIMP_NEW_ROOT}")
        message(WARNING "  Missing: cmake-modules/assimp-plain-config.cmake.in")
        message(WARNING "  Checked for pre-built libs in:")
        message(WARNING "    - ${ASSIMP_LIB_DIR}")
        message(WARNING "    - ${ASSIMP_NEW_ROOT}/lib")
        message(WARNING "    - ${ASSIMP_OLD_ROOT}/lib")
        message(WARNING "    - ${GAME_LIBS_DIR}/assimp/lib")
        message(WARNING "  Model loading will not be available")
        message(WARNING "  To fix: Download complete Assimp source or add pre-built libraries")
    endif()
endif()

# Platform-specific libraries
if(WIN32)
    target_link_libraries(Platform PUBLIC winmm)
endif()

message(STATUS "")
message(STATUS "Platform Library Configuration:")
message(STATUS "  Render sources: ${PLATFORM_SOURCES}")
message(STATUS "  GLEW: ${GLEW_ROOT}")
message(STATUS "  GLFW: ${GLFW_ROOT} (building from source)")
message(STATUS "  GLM: ${GLM_ROOT}")
if(ASSIMP_HAS_COMPLETE_SOURCE)
    message(STATUS "  Assimp: ${ASSIMP_NEW_ROOT} (building from source)")
else()
    message(STATUS "  Assimp: Using pre-built libraries or not available")
endif()
message(STATUS "")
