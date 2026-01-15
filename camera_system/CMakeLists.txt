cmake_minimum_required(VERSION 3.22)

# Camera System Editor Executable
project(CameraSystemEditor)

# Set architecture for macOS builds (default to native architecture)
# This must be set BEFORE any subdirectories are added to ensure consistent architecture
if(APPLE)
    # Detect native architecture
    execute_process(
        COMMAND uname -m
        OUTPUT_VARIABLE NATIVE_ARCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NATIVE_ARCH STREQUAL "arm64")
        if(NOT DEFINED CMAKE_OSX_ARCHITECTURES OR CMAKE_OSX_ARCHITECTURES STREQUAL "")
            set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures for macOS" FORCE)
            message(STATUS "Setting CMAKE_OSX_ARCHITECTURES to arm64 (native architecture)")
        else()
            message(STATUS "CMAKE_OSX_ARCHITECTURES is already set to: ${CMAKE_OSX_ARCHITECTURES}")
        endif()
    endif()
endif()

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
# Ouster SDK Integration
# ============================================================================

set(OUSTER_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ouster/ouster-sdk")
if(EXISTS "${OUSTER_SDK_DIR}/CMakeLists.txt")
    message(STATUS "Found Ouster SDK at: ${OUSTER_SDK_DIR}")
    
    # Check if we can build SDK dependencies (need arm64 libraries for arm64 builds)
    # If dependencies are missing, we'll fall back to manual parsing
    set(SDK_DEPS_AVAILABLE TRUE)
    if(APPLE AND (CMAKE_OSX_ARCHITECTURES MATCHES "arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64"))
        # For arm64 builds, verify we have arm64 libraries in /opt/homebrew/
        if(BREW_PREFIX STREQUAL "/opt/homebrew")
            # Check if libraries exist in /opt/homebrew/
            if(NOT EXISTS "${BREW_PREFIX}/opt/openssl@3/lib/libssl.dylib")
                message(WARNING 
                    "Ouster SDK dependencies not found in /opt/homebrew/!\n"
                    "PCAP loading will use manual parsing instead.\n"
                    "To enable SDK support, install dependencies:\n"
                    "  /opt/homebrew/bin/brew install openssl@3 libzip libpcap libtins eigen\n"
                    "Or if you have Homebrew in /opt/homebrew, run:\n"
                    "  brew install openssl@3 libzip libpcap libtins eigen"
                )
                set(SDK_DEPS_AVAILABLE FALSE)
            else()
                # For arm64 builds, restrict search paths BEFORE SDK CMake runs
                # Remove /usr/local from system paths to prevent finding x86_64 libraries
                list(REMOVE_ITEM CMAKE_SYSTEM_PREFIX_PATH "/usr/local")
                list(REMOVE_ITEM CMAKE_SYSTEM_FRAMEWORK_PATH "/usr/local/Frameworks")
                # Prepend /opt/homebrew paths to ensure they're found first
                list(INSERT CMAKE_PREFIX_PATH 0 "${BREW_PREFIX}/opt/openssl@3" "${BREW_PREFIX}/opt/libzip" "${BREW_PREFIX}/opt/libpcap" "${BREW_PREFIX}/opt/libtins" "${BREW_PREFIX}")
                set(CMAKE_LIBRARY_PATH "${BREW_PREFIX}/lib" "${BREW_PREFIX}/opt/openssl@3/lib" "${BREW_PREFIX}/opt/libzip/lib" "${BREW_PREFIX}/opt/libpcap/lib" "${BREW_PREFIX}/opt/libtins/lib" ${CMAKE_LIBRARY_PATH})
            endif()
        elseif(BREW_PREFIX STREQUAL "/usr/local")
            message(WARNING "Homebrew in /usr/local/ detected - libraries may be x86_64")
            message(WARNING "For arm64 builds with SDK support, you may need: arch -arm64 brew reinstall openssl@3 libzip libpcap libtins eigen")
            message(WARNING "Or install native arm64 Homebrew in /opt/homebrew/")
            message(WARNING "PCAP loading will use manual parsing if SDK dependencies are not available")
        endif()
    endif()
    
    if(SDK_DEPS_AVAILABLE)
        # Set C++ standard for SDK (requires C++17 for robin-map compatibility)
        if(NOT DEFINED CMAKE_CXX_STANDARD)
            set(CMAKE_CXX_STANDARD 17)
            set(CMAKE_CXX_STANDARD_REQUIRED ON)
        else()
            # If set from command line, ensure it's at least 17 for robin-map
            if(CMAKE_CXX_STANDARD VERSION_LESS 17)
                message(WARNING "CMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD} is less than 17, which is required for robin-map. Setting to 17.")
                set(CMAKE_CXX_STANDARD 17)
            endif()
            set(CMAKE_CXX_STANDARD_REQUIRED ON)
        endif()
        
        # Set architecture before adding SDK subdirectory to ensure consistent build
        # Force arm64 if we're on an arm64 system to avoid x86_64 build issues
        if(APPLE)
            execute_process(
                COMMAND uname -m
                OUTPUT_VARIABLE NATIVE_ARCH
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            if(NATIVE_ARCH STREQUAL "arm64")
                # Force arm64 architecture for SDK build
                set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures for macOS" FORCE)
                message(STATUS "Forcing CMAKE_OSX_ARCHITECTURES to arm64 for SDK build (native: ${NATIVE_ARCH})")
            endif()
        endif()
        
        # Add Ouster SDK as a subdirectory (but don't build everything)
        set(BUILD_SENSOR ON CACHE BOOL "Build Ouster Sensor library" FORCE)  # Enable for live streaming
        set(BUILD_PCAP ON CACHE BOOL "Build pcap utils" FORCE)
        
        # Define BUILD_SENSOR for the compiler so we can conditionally include sensor headers
        add_compile_definitions(BUILD_SENSOR)
        set(BUILD_OSF OFF CACHE BOOL "Build Ouster OSF library" FORCE)
        set(BUILD_VIZ OFF CACHE BOOL "Build Ouster visualizer" FORCE)
        set(BUILD_MAPPING OFF CACHE BOOL "Build mapping" FORCE)
        set(BUILD_PYTHON_MODULE OFF CACHE BOOL "Build python module" FORCE)
        set(BUILD_TESTING OFF CACHE BOOL "Build tests" FORCE)
        set(BUILD_EXAMPLES OFF CACHE BOOL "Build C++ examples" FORCE)
        set(BUILD_SHARED_LIBRARY OFF CACHE BOOL "Build shared Library" FORCE)
        
        # Eigen3 is required by ouster_client
        # Try to find Eigen3 via Homebrew first, fall back to local_deps if needed
        if(USE_HOMEBREW)
            set(EIGEN3_HOMEBREW_PATH "${BREW_PREFIX}/opt/eigen")
            if(EXISTS "${EIGEN3_HOMEBREW_PATH}/include/eigen3")
                list(APPEND CMAKE_PREFIX_PATH "${EIGEN3_HOMEBREW_PATH}")
                message(STATUS "Using Eigen3 from Homebrew: ${EIGEN3_HOMEBREW_PATH}")
            endif()
        endif()
        
        # Fall back to local_deps if Homebrew Eigen3 not found
        set(EIGEN3_LOCAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/ouster/local_deps/eigen3")
        if(NOT EIGEN3_HOMEBREW_PATH AND EXISTS "${EIGEN3_LOCAL_PATH}")
            list(APPEND CMAKE_PREFIX_PATH "${EIGEN3_LOCAL_PATH}")
            message(STATUS "Using Eigen3 from local_deps: ${EIGEN3_LOCAL_PATH}")
        elseif(NOT EIGEN3_HOMEBREW_PATH AND NOT EXISTS "${EIGEN3_LOCAL_PATH}")
            message(WARNING "Eigen3 not found in Homebrew or local_deps - SDK may fail to build")
            message(WARNING "Install Eigen3 via: /opt/homebrew/bin/brew install eigen")
        endif()
        
        # Add SDK's cmake modules to CMAKE_MODULE_PATH (for FindPcap.cmake, etc.)
        list(APPEND CMAKE_MODULE_PATH "${OUSTER_SDK_DIR}/cmake")
        
        add_subdirectory(${OUSTER_SDK_DIR} ouster_sdk_build EXCLUDE_FROM_ALL)
        
        # Ensure ouster_sensor is built (even though SDK is EXCLUDE_FROM_ALL)
        if(TARGET ouster_sensor)
            # Add the sensor library to the build (it's needed for live streaming)
            set_target_properties(ouster_sensor PROPERTIES EXCLUDE_FROM_ALL FALSE)
        endif()
        
        # Fix libtins include directories after SDK is configured
        # The SDK's Findlibtins.cmake may not set include dirs correctly, especially for /opt/homebrew/
        if(TARGET tins)
            # Find the actual include directory
            if(APPLE AND (CMAKE_OSX_ARCHITECTURES MATCHES "arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64"))
                if(EXISTS "/opt/homebrew/include/tins/tins.h")
                    set(LIBTINS_INCLUDE_DIR "/opt/homebrew/include")
                elseif(EXISTS "/opt/homebrew/Cellar/libtins")
                    # Find the versioned directory
                    file(GLOB LIBTINS_CELLAR_DIRS "/opt/homebrew/Cellar/libtins/*/include")
                    if(LIBTINS_CELLAR_DIRS)
                        list(GET LIBTINS_CELLAR_DIRS 0 LIBTINS_INCLUDE_DIR)
                    endif()
                endif()
            endif()
            
            # If not found above, try standard search
            if(NOT LIBTINS_INCLUDE_DIR)
                find_path(LIBTINS_INCLUDE_DIR NAMES tins/tins.h PATHS 
                    "/opt/homebrew/include"
                    "/usr/local/include"
                    "/usr/include"
                    NO_DEFAULT_PATH)
                if(NOT LIBTINS_INCLUDE_DIR)
                    find_path(LIBTINS_INCLUDE_DIR NAMES tins/tins.h)
                endif()
            endif()
            
            if(LIBTINS_INCLUDE_DIR)
                message(STATUS "Setting libtins include directory: ${LIBTINS_INCLUDE_DIR}")
                # Set include dirs on tins target
                set_target_properties(tins PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${LIBTINS_INCLUDE_DIR}")
                
                # Also ensure libtins target has include dirs (ouster_pcap links to libtins::libtins)
                if(TARGET libtins)
                    set_target_properties(libtins PROPERTIES
                        INTERFACE_INCLUDE_DIRECTORIES "${LIBTINS_INCLUDE_DIR}")
                endif()
            else()
                message(WARNING "Could not find libtins include directory - compilation may fail")
            endif()
        endif()
        
        set(OUSTER_SDK_AVAILABLE TRUE)
        message(STATUS "Ouster SDK integration enabled")
    else()
        set(OUSTER_SDK_AVAILABLE FALSE)
        message(WARNING "Ouster SDK dependencies not available - PCAP loading will use manual parsing")
    endif()
else()
    set(OUSTER_SDK_AVAILABLE FALSE)
    message(WARNING "Ouster SDK not found at ${OUSTER_SDK_DIR} - PCAP loading will use manual parsing")
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
    ouster/OusterStream.cpp
    ouster/OusterStream.h
    ouster/OusterStreamManual.cpp
    gear360/Gear360Stream.cpp
    gear360/Gear360Stream.h
    main.cpp
)

# Create executable
add_executable(camera_system_editor ${SOURCES})

# Set Objective-C++ compilation for Gear360Stream on macOS
if(APPLE)
    set_source_files_properties(gear360/Gear360Stream.cpp PROPERTIES
        LANGUAGE OBJCXX
    )
endif()

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

# Link Ouster SDK libraries if available
if(OUSTER_SDK_AVAILABLE AND TARGET ouster_client AND TARGET ouster_pcap)
        # Add SDK include directories explicitly
        target_include_directories(camera_system_editor PRIVATE
            ${OUSTER_SDK_DIR}/ouster_client/include
            ${OUSTER_SDK_DIR}/ouster_pcap/include
        )
        
        # Add sensor library include directory if sensor library is built
        if(TARGET ouster_sensor)
            target_include_directories(camera_system_editor PRIVATE
                ${OUSTER_SDK_DIR}/ouster_sensor/include
            )
        endif()
    
    # Find and link SDK dependencies (needed because SDK uses static libs with PRIVATE deps)
    # The SDK libraries have PRIVATE dependencies that need to be linked explicitly
    
    # Link SDK libraries first
    target_link_libraries(camera_system_editor 
        ouster_client
        ouster_pcap
    )
    
    # Link sensor library if available (for live streaming)
    if(TARGET ouster_sensor)
        target_link_libraries(camera_system_editor ouster_sensor)
    endif()
    
    # Explicitly link dependencies (static libs need transitive deps)
    # The SDK libraries have PRIVATE dependencies, so we need to link them explicitly
    # CRITICAL: For arm64 builds, we must use arm64 libraries from /opt/homebrew/
    # and exclude /usr/local/ to avoid x86_64 libraries
    
    # For arm64 builds, restrict search to BREW_PREFIX only to avoid x86_64 libraries
    if(APPLE AND (CMAKE_OSX_ARCHITECTURES MATCHES "arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64"))
        if(BREW_PREFIX STREQUAL "/opt/homebrew")
            # Only search in /opt/homebrew/ for arm64 builds
            set(CMAKE_PREFIX_PATH_SAVE ${CMAKE_PREFIX_PATH})
            set(CMAKE_PREFIX_PATH "${BREW_PREFIX}/opt/openssl@3" "${BREW_PREFIX}/opt/libzip" "${BREW_PREFIX}/opt/libpcap" "${BREW_PREFIX}/opt/libtins" "${BREW_PREFIX}")
            set(CMAKE_LIBRARY_PATH_SAVE ${CMAKE_LIBRARY_PATH})
            set(CMAKE_LIBRARY_PATH "${BREW_PREFIX}/lib" "${BREW_PREFIX}/opt/openssl@3/lib" "${BREW_PREFIX}/opt/libzip/lib" "${BREW_PREFIX}/opt/libpcap/lib" "${BREW_PREFIX}/opt/libtins/lib")
            # Remove /usr/local from search paths
            list(REMOVE_ITEM CMAKE_SYSTEM_PREFIX_PATH "/usr/local")
            list(REMOVE_ITEM CMAKE_SYSTEM_FRAMEWORK_PATH "/usr/local/Frameworks")
        endif()
    endif()
    
    # Find SDK dependencies (optional - if not found, we'll skip linking but build will continue)
    find_package(OpenSSL QUIET)
    find_package(libzip QUIET)
    find_package(Pcap QUIET)
    find_package(libtins QUIET)
    
    # Check if all dependencies were found
    if(NOT OpenSSL_FOUND OR NOT libzip_FOUND OR NOT Pcap_FOUND OR NOT libtins_FOUND)
        message(WARNING "Some Ouster SDK dependencies not found - SDK linking may fail")
        message(WARNING "Missing: OpenSSL=${OpenSSL_FOUND}, libzip=${libzip_FOUND}, Pcap=${Pcap_FOUND}, libtins=${libtins_FOUND}")
    endif()
    
    # Restore original paths
    if(APPLE AND (CMAKE_OSX_ARCHITECTURES MATCHES "arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64"))
        if(BREW_PREFIX STREQUAL "/opt/homebrew" AND DEFINED CMAKE_PREFIX_PATH_SAVE)
            set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH_SAVE})
            set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH_SAVE})
        endif()
    endif()
    
    # Explicitly link dependencies (only if found)
    if(OpenSSL_FOUND)
        if(TARGET OpenSSL::SSL)
            target_link_libraries(camera_system_editor OpenSSL::SSL OpenSSL::Crypto)
        elseif(OPENSSL_LIBRARIES)
            target_link_libraries(camera_system_editor ${OPENSSL_LIBRARIES})
        endif()
    endif()
    
    if(libzip_FOUND AND TARGET libzip::zip)
        target_link_libraries(camera_system_editor libzip::zip)
    endif()
    
    if(Pcap_FOUND AND TARGET libpcap::libpcap)
        target_link_libraries(camera_system_editor libpcap::libpcap)
    endif()
    
    if(libtins_FOUND AND TARGET libtins::libtins)
        target_link_libraries(camera_system_editor libtins::libtins)
    endif()
    
    # Check architecture compatibility
    if(APPLE)
        # Check if we're building for arm64
        if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            message(STATUS "Building for arm64 architecture")
            if(BREW_PREFIX STREQUAL "/opt/homebrew")
                # Verify libraries exist in /opt/homebrew/ (this is just a warning since we're already in the SDK block)
                if(NOT EXISTS "${BREW_PREFIX}/opt/openssl@3/lib/libssl.dylib")
                    message(WARNING 
                        "arm64 libraries not found in /opt/homebrew/!\n"
                        "SDK linking may fail. Install them using native arm64 Homebrew:\n"
                        "  /opt/homebrew/bin/brew install openssl@3 libzip libpcap libtins\n"
                        "Or if you have Homebrew in /opt/homebrew, run:\n"
                        "  brew install openssl@3 libzip libpcap libtins"
                    )
                endif()
            else()
                message(WARNING "If you see linker errors about 'found architecture x86_64', you need arm64 libraries.")
                message(WARNING "Reinstall libraries for arm64: arch -arm64 brew reinstall openssl@3 libzip libpcap libtins")
            endif()
        endif()
    endif()
    target_compile_definitions(camera_system_editor PRIVATE OUSTER_SDK_AVAILABLE)
    message(STATUS "Linked Ouster SDK libraries to camera_system_editor")
    if(TARGET ouster_sensor)
        message(STATUS "Ouster SDK include dirs: ${OUSTER_SDK_DIR}/ouster_client/include, ${OUSTER_SDK_DIR}/ouster_pcap/include, ${OUSTER_SDK_DIR}/ouster_sensor/include")
        message(STATUS "Ouster SDK live streaming enabled via ouster_sensor")
    else()
        message(STATUS "Ouster SDK include dirs: ${OUSTER_SDK_DIR}/ouster_client/include, ${OUSTER_SDK_DIR}/ouster_pcap/include")
        message(STATUS "Ouster SDK live streaming not available (ouster_sensor not built)")
    endif()
else()
    message(WARNING "Ouster SDK libraries not available - PCAP loading will use manual parsing")
endif()

# macOS-specific frameworks and libraries
if(APPLE)
    find_package(OpenGL REQUIRED)
    target_link_libraries(camera_system_editor OpenGL::GL)
    
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(IOKIT_FRAMEWORK IOKit)
    find_library(COREVIDEO_FRAMEWORK CoreVideo)
    find_library(COREBLUETOOTH_FRAMEWORK CoreBluetooth)
    find_library(FOUNDATION_FRAMEWORK Foundation)
    if(COCOA_FRAMEWORK AND IOKIT_FRAMEWORK AND COREVIDEO_FRAMEWORK)
        target_link_libraries(camera_system_editor 
            ${COCOA_FRAMEWORK} 
            ${IOKIT_FRAMEWORK} 
            ${COREVIDEO_FRAMEWORK}
        )
    endif()
    if(COREBLUETOOTH_FRAMEWORK AND FOUNDATION_FRAMEWORK)
        target_link_libraries(camera_system_editor 
            ${COREBLUETOOTH_FRAMEWORK}
            ${FOUNDATION_FRAMEWORK}
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

# ============================================================================
# Gear 360 Test Application
# ============================================================================

set(GEAR360_TEST_SOURCES
    gear360/Gear360Stream.cpp
    gear360/Gear360Stream.h
    gear360/gear360_test.cpp
)

add_executable(gear360_test ${GEAR360_TEST_SOURCES})

# Set output directory
set_target_properties(gear360_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Link libraries (same as camera_system_editor)
if(GLEW_LIB)
    target_link_libraries(gear360_test ${GLEW_LIB})
endif()

if(GLFW_LIB)
    target_link_libraries(gear360_test ${GLFW_LIB})
endif()

# Link platform library if it exists
if(TARGET Platform)
    target_link_libraries(gear360_test Platform)
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
    
    # Set Objective-C++ compilation for Gear360Stream
    set_source_files_properties(gear360/Gear360Stream.cpp PROPERTIES
        LANGUAGE OBJCXX
    )
endif()

# Compiler options
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

