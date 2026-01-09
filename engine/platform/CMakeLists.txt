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
# Rendering Dependencies
# ============================================================================

# Detect platform and set up dependency paths
if(APPLE)
    # macOS: Use Homebrew paths
    if(DEFINED HOMEBREW_PREFIX)
        set(BREW_PREFIX "${HOMEBREW_PREFIX}")
    elseif(EXISTS "/opt/homebrew")
        set(BREW_PREFIX "/opt/homebrew")  # Apple Silicon
    else()
        set(BREW_PREFIX "/usr/local")  # Intel Mac
    endif()
    message(STATUS "Using Homebrew prefix: ${BREW_PREFIX}")
    set(USE_HOMEBREW TRUE)
else()
    set(USE_HOMEBREW FALSE)
endif()

# Set base path to game_libs directory (Windows fallback)
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
set(GLEW_FOUND_LOCAL FALSE)

if(USE_HOMEBREW)
    # macOS: Use Homebrew GLEW
    set(GLEW_INCLUDE_DIR "${BREW_PREFIX}/opt/glew/include")
    set(GLEW_LIB_DIR "${BREW_PREFIX}/opt/glew/lib")
    
    if(EXISTS "${GLEW_INCLUDE_DIR}/GL/glew.h")
        target_include_directories(Platform PUBLIC ${GLEW_INCLUDE_DIR})
        # Link to the dylib on macOS
        find_library(GLEW_LIBRARY GLEW PATHS "${GLEW_LIB_DIR}" NO_DEFAULT_PATH)
        if(GLEW_LIBRARY)
            target_link_libraries(Platform PUBLIC ${GLEW_LIBRARY})
            message(STATUS "Found GLEW (Homebrew): ${GLEW_LIBRARY}")
            set(GLEW_FOUND_LOCAL TRUE)
        endif()
    endif()
else()
    # Windows: Use game_libs directory
set(GLEW_ROOT "${GAME_LIBS_DIR}/glew-2.1.0")
set(GLEW_INCLUDE_DIR "${GLEW_ROOT}/include")
set(GLEW_LIB_DIR "${GLEW_ROOT}/lib/Release/x64")
set(GLEW_DLL_DIR "${GLEW_ROOT}/bin/Release/x64")

if(EXISTS "${GLEW_INCLUDE_DIR}/GL/glew.h" AND EXISTS "${GLEW_LIB_DIR}/glew32.lib")
    target_include_directories(Platform PUBLIC ${GLEW_INCLUDE_DIR})
    target_link_libraries(Platform PUBLIC "${GLEW_LIB_DIR}/glew32.lib")
    message(STATUS "Found GLEW: ${GLEW_ROOT}")
        set(GLEW_FOUND_LOCAL TRUE)
    
    # Copy DLL to output directory
    if(EXISTS "${GLEW_DLL_DIR}/glew32.dll")
        add_custom_command(TARGET Platform POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GLEW_DLL_DIR}/glew32.dll"
            "$<TARGET_FILE_DIR:Platform>"
            COMMENT "Copying GLEW DLL to output directory"
        )
    endif()
    endif()
endif()

if(NOT GLEW_FOUND_LOCAL)
    message(WARNING "GLEW not found")
endif()

# GLFW for windowing
set(GLFW_FOUND_LOCAL FALSE)

if(USE_HOMEBREW)
    # macOS: Use Homebrew GLFW
    set(GLFW_INCLUDE_DIR "${BREW_PREFIX}/opt/glfw/include")
    set(GLFW_LIB_DIR "${BREW_PREFIX}/opt/glfw/lib")
    
    if(EXISTS "${GLFW_INCLUDE_DIR}/GLFW/glfw3.h")
        target_include_directories(Platform PUBLIC ${GLFW_INCLUDE_DIR})
        find_library(GLFW_LIBRARY glfw PATHS "${GLFW_LIB_DIR}" NO_DEFAULT_PATH)
        if(GLFW_LIBRARY)
            target_link_libraries(Platform PUBLIC ${GLFW_LIBRARY})
            message(STATUS "Found GLFW (Homebrew): ${GLFW_LIBRARY}")
            set(GLFW_FOUND_LOCAL TRUE)
        endif()
    endif()
else()
    # Windows: Build from source
set(GLFW_ROOT "${GAME_LIBS_DIR}/glfw-3.4")
set(GLFW_INCLUDE_DIR "${GLFW_ROOT}/include")

if(EXISTS "${GLFW_INCLUDE_DIR}/GLFW/glfw3.h")
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    
    add_subdirectory("${GLFW_ROOT}" "${CMAKE_BINARY_DIR}/glfw" EXCLUDE_FROM_ALL)
    target_include_directories(glfw INTERFACE ${GLFW_INCLUDE_DIR})
    target_link_libraries(Platform PUBLIC glfw)
    message(STATUS "Found GLFW (building from source): ${GLFW_ROOT}")
        set(GLFW_FOUND_LOCAL TRUE)
    endif()
endif()

if(NOT GLFW_FOUND_LOCAL)
    message(WARNING "GLFW not found")
endif()

# GLM for math (header-only)
set(GLM_FOUND_LOCAL FALSE)

if(USE_HOMEBREW)
    # macOS: Use Homebrew GLM
    set(GLM_INCLUDE_DIR "${BREW_PREFIX}/opt/glm/include")
    
    if(EXISTS "${GLM_INCLUDE_DIR}/glm/glm.hpp")
        target_include_directories(Platform PUBLIC ${GLM_INCLUDE_DIR})
        message(STATUS "Found GLM (Homebrew): ${GLM_INCLUDE_DIR}")
        set(GLM_FOUND_LOCAL TRUE)
    endif()
else()
    # Windows: Use game_libs directory
set(GLM_ROOT "${GAME_LIBS_DIR}/glm-1.0.2")
set(GLM_INCLUDE_DIR "${GLM_ROOT}")

if(EXISTS "${GLM_INCLUDE_DIR}/glm/glm.hpp")
    target_include_directories(Platform PUBLIC ${GLM_INCLUDE_DIR})
    message(STATUS "Found GLM: ${GLM_ROOT}")
        set(GLM_FOUND_LOCAL TRUE)
    endif()
endif()

if(NOT GLM_FOUND_LOCAL)
    message(WARNING "GLM not found")
endif()

# Assimp for model loading
set(ASSIMP_FOUND_LOCAL FALSE)

if(USE_HOMEBREW)
    # macOS: Use Homebrew Assimp
    set(ASSIMP_INCLUDE_DIR "${BREW_PREFIX}/opt/assimp/include")
    set(ASSIMP_LIB_DIR "${BREW_PREFIX}/opt/assimp/lib")
    
    if(EXISTS "${ASSIMP_INCLUDE_DIR}/assimp/Importer.hpp")
        target_include_directories(Platform PUBLIC ${ASSIMP_INCLUDE_DIR})
        find_library(ASSIMP_LIBRARY assimp PATHS "${ASSIMP_LIB_DIR}" NO_DEFAULT_PATH)
        if(ASSIMP_LIBRARY)
            target_link_libraries(Platform PUBLIC ${ASSIMP_LIBRARY})
            message(STATUS "Found Assimp (Homebrew): ${ASSIMP_LIBRARY}")
            set(ASSIMP_FOUND_LOCAL TRUE)
        endif()
    endif()
else()
    # Windows: Try new 6.0.2 location first
set(ASSIMP_NEW_ROOT "${GAME_LIBS_DIR}/assimp-6.0.2")
set(ASSIMP_OLD_ROOT "${GAME_LIBS_DIR}/assimp")
    set(ASSIMP_LIB_DIR "${GAME_LIBS_DIR}/assimp_lib")
set(ASSIMP_INCLUDE_DIR "${ASSIMP_NEW_ROOT}/include")

set(ASSIMP_CMAKE_MODULES_DIR "${ASSIMP_NEW_ROOT}/cmake-modules")
set(ASSIMP_HAS_COMPLETE_SOURCE FALSE)

if(EXISTS "${ASSIMP_INCLUDE_DIR}/assimp/Importer.hpp" 
   AND EXISTS "${ASSIMP_NEW_ROOT}/CMakeLists.txt"
   AND EXISTS "${ASSIMP_NEW_ROOT}/contrib/zlib")
    if(EXISTS "${ASSIMP_CMAKE_MODULES_DIR}/assimp-plain-config.cmake.in")
        set(ASSIMP_HAS_COMPLETE_SOURCE TRUE)
    endif()
endif()

if(ASSIMP_HAS_COMPLETE_SOURCE)
    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
    
    add_subdirectory("${ASSIMP_NEW_ROOT}" "${CMAKE_BINARY_DIR}/assimp" EXCLUDE_FROM_ALL)
    target_link_libraries(Platform PUBLIC assimp)
    target_include_directories(Platform PUBLIC ${ASSIMP_INCLUDE_DIR})
    
    message(STATUS "Found Assimp 6.0.2 source: ${ASSIMP_NEW_ROOT}")
        message(STATUS "  Building from source (may take a few minutes on first build)")
        set(ASSIMP_FOUND_LOCAL TRUE)
else()
        message(STATUS "Assimp source incomplete, trying pre-built libraries...")
    
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
    
    if(ASSIMP_INCLUDE_DIR_FOUND AND ASSIMP_LIBRARY)
        target_include_directories(Platform PUBLIC ${ASSIMP_INCLUDE_DIR_FOUND})
        target_link_libraries(Platform PUBLIC ${ASSIMP_LIBRARY})
        message(STATUS "Found Assimp (pre-built): ${ASSIMP_INCLUDE_DIR_FOUND}")
            set(ASSIMP_FOUND_LOCAL TRUE)
        
            # Copy DLL if it exists
            set(ASSIMP_DLL "${ASSIMP_LIB_DIR}/assimp-vc143-mt.dll")
            if(NOT EXISTS "${ASSIMP_DLL}")
                set(ASSIMP_DLL "${ASSIMP_OLD_ROOT}/assimp-vc143-mt.dll")
            endif()
        if(EXISTS "${ASSIMP_DLL}")
            add_custom_command(TARGET Platform POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ASSIMP_DLL}"
                $<TARGET_FILE_DIR:Game>
            )
            endif()
        endif()
    endif()
endif()

if(NOT ASSIMP_FOUND_LOCAL)
    message(WARNING "Assimp not found - model loading will not be available")
endif()

# Platform-specific libraries
if(WIN32)
    target_link_libraries(Platform PUBLIC winmm)
endif()

# macOS-specific frameworks
if(APPLE)
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(IOKIT_FRAMEWORK IOKit)
    find_library(COREVIDEO_FRAMEWORK CoreVideo)
    if(COCOA_FRAMEWORK AND IOKIT_FRAMEWORK AND COREVIDEO_FRAMEWORK)
        target_link_libraries(Platform PUBLIC ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${COREVIDEO_FRAMEWORK})
    endif()
endif()

message(STATUS "")
message(STATUS "Platform Library Configuration:")
message(STATUS "  Render sources: ${PLATFORM_SOURCES}")
if(USE_HOMEBREW)
    message(STATUS "  Using Homebrew dependencies from: ${BREW_PREFIX}")
    message(STATUS "  GLEW: ${GLEW_FOUND_LOCAL}")
    message(STATUS "  GLFW: ${GLFW_FOUND_LOCAL}")
    message(STATUS "  GLM: ${GLM_FOUND_LOCAL}")
    message(STATUS "  Assimp: ${ASSIMP_FOUND_LOCAL}")
else()
    message(STATUS "  GLEW: ${GLEW_ROOT}")
    message(STATUS "  GLFW: ${GLFW_ROOT}")
    message(STATUS "  GLM: ${GLM_ROOT}")
    message(STATUS "  Assimp: ${ASSIMP_NEW_ROOT}")
endif()
message(STATUS "")
