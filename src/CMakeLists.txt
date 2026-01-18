# Game Executable - Final player executable

# ============================================================================
# Game Executable
# ============================================================================

# Find game source files
file(GLOB GAME_SOURCES
    "*.cpp"
    "*.h"
)

# If no game sources exist yet, create a placeholder main
if(NOT GAME_SOURCES)
    # Create a simple main.cpp as starting point
    message(STATUS "No game sources found. Creating placeholder main.cpp")
    set(GAME_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
endif()

# Create game executable
add_executable(Game ${GAME_SOURCES})

# Add src directory to include path so game systems can be included
target_include_directories(Game PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link to engine and platform libraries
target_link_libraries(Game PRIVATE 
    Engine
    Platform
)

# Copy GLEW DLL to Game executable directory (Game links to Platform which uses GLEW)
set(GLEW_DLL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../game_libs/glew-2.1.0/bin/Release/x64")
if(EXISTS "${GLEW_DLL_DIR}/glew32.dll")
    add_custom_command(TARGET Game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${GLEW_DLL_DIR}/glew32.dll"
        "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
        COMMENT "Copying GLEW DLL to Game output directory"
    )
endif()

# Set output name - CMake automatically adds .exe on Windows
set_target_properties(Game PROPERTIES
    OUTPUT_NAME "game"
)

# Set working directory for running from IDE
set_target_properties(Game PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

# Copy shader files to build directory so they can be found at runtime
file(GLOB SHADER_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../RenderingEngine/platform/render/shaders/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/../RenderingEngine/platform/render/shaders/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/../RenderingEngine/platform/render/shaders/*.geom"
)

# Copy shaders to the same directory as the executable (using generator expression)
# This ensures shaders are accessible at runtime relative to the executable
add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Game>/shaders"
    COMMENT "Creating shaders directory"
)

foreach(SHADER ${SHADER_FILES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    add_custom_command(TARGET Game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SHADER}"
            "$<TARGET_FILE_DIR:Game>/shaders/${SHADER_NAME}"
        COMMENT "Copying ${SHADER_NAME} to build directory"
    )
endforeach()

# ============================================================================
# Copy Graphics Assets
# ============================================================================

# Copy the entire graphics directory to build output directory
# This ensures assets are accessible at runtime relative to the executable
set(GRAPHICS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../graphics")

# Check if graphics directory exists
if(EXISTS "${GRAPHICS_SOURCE_DIR}")
    message(STATUS "Graphics directory found: ${GRAPHICS_SOURCE_DIR}")
    
    # Copy graphics directory recursively after building Game target
    # Use generator expression to copy to the same directory as the executable
    # This works for both single-config (Unix) and multi-config (Visual Studio) generators
    add_custom_command(TARGET Game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Copying graphics assets to build directory..."
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Game>/graphics"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${GRAPHICS_SOURCE_DIR}" "$<TARGET_FILE_DIR:Game>/graphics"
        COMMENT "Copying graphics assets to $<TARGET_FILE_DIR:Game>/graphics"
    )
    
    message(STATUS "  Graphics will be copied to: $<TARGET_FILE_DIR:Game>/graphics")
else()
    message(WARNING "Graphics directory not found at ${GRAPHICS_SOURCE_DIR}")
    message(WARNING "  Model loading may fail at runtime")
endif()

# ============================================================================
# Game-specific Configuration
# ============================================================================

# Add any game-specific compile definitions
# target_compile_definitions(Game PRIVATE
#     GAME_VERSION="${PROJECT_VERSION}"
# )

