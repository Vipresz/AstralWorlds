# ML Feature Extractor for DensitySDF Training
cmake_minimum_required(VERSION 3.16)
project(ml_extractor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find GLM (header-only library)
find_package(glm QUIET)
if(NOT glm_FOUND)
    # Try to find glm in common locations
    find_path(GLM_INCLUDE_DIR glm/glm.hpp
        PATHS
        /usr/include
        /usr/local/include
        /opt/homebrew/include
        ${CMAKE_SOURCE_DIR}/../engine/external/glm
        ${CMAKE_SOURCE_DIR}/../external/glm
    )
    if(GLM_INCLUDE_DIR)
        message(STATUS "Found GLM at: ${GLM_INCLUDE_DIR}")
        include_directories(${GLM_INCLUDE_DIR})
    else()
        message(FATAL_ERROR "GLM not found. Please install glm or set GLM_INCLUDE_DIR")
    endif()
else()
    message(STATUS "Found GLM via package")
endif()

# Source files
set(SOURCES
    main.cpp
    FeatureExtractor.cpp
)

set(HEADERS
    FeatureExtractor.h
)

# Create executable
add_executable(ml_extractor ${SOURCES} ${HEADERS})

# Link libraries
if(glm_FOUND)
    target_link_libraries(ml_extractor PRIVATE glm::glm)
endif()

# Set output directory
set_target_properties(ml_extractor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Installation
install(TARGETS ml_extractor DESTINATION bin)

# Print configuration
message(STATUS "ML Extractor Configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
