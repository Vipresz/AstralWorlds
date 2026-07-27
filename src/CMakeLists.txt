# Game Executable - Final player executable

# ============================================================================
# Game Executable
# ============================================================================

# Find game source files
file(GLOB GAME_SOURCES
    "*.cpp"
    "*.h"
    "character/*.cpp"
    "character/*.h"
    "ui/*.cpp"
    "ui/*.h"
    "menus/main_menu/src/*.cpp"
    "menus/main_menu/src/*.h"
    "menus/src/*.cpp"
    "menus/src/*.h"
)

if(NOT GAME_SOURCES)
    message(STATUS "No game sources found. Creating placeholder main.cpp")
    set(GAME_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
endif()

# Create game executable
add_executable(Game ${GAME_SOURCES})

# Add src directory to include path so game systems can be included
target_include_directories(Game PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/menus/main_menu/src
    ${CMAKE_CURRENT_SOURCE_DIR}/menus/src
)

# Link to engine and platform libraries
target_link_libraries(Game PRIVATE
    Engine
    Platform
)

if(DEFINED NUKLEAR_DIR)
    target_include_directories(Game PRIVATE ${NUKLEAR_DIR})
endif()

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

set(GRAPHICS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../graphics")

if(EXISTS "${GRAPHICS_SOURCE_DIR}")
    message(STATUS "Graphics directory found: ${GRAPHICS_SOURCE_DIR}")

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

# Copy terrain assets (heightmaps, etc.)
set(TERRAIN_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../terrain")
if(EXISTS "${TERRAIN_SOURCE_DIR}")
    add_custom_command(TARGET Game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Game>/terrain"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${TERRAIN_SOURCE_DIR}" "$<TARGET_FILE_DIR:Game>/terrain"
        COMMENT "Copying terrain assets to build directory"
    )
    message(STATUS "Terrain directory found: ${TERRAIN_SOURCE_DIR}")
endif()

# Copy main menu assets (buttons, fonts, etc.)
set(MENUS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/menus")
if(EXISTS "${MENUS_SOURCE_DIR}")
    add_custom_command(TARGET Game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Game>/menus"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${MENUS_SOURCE_DIR}" "$<TARGET_FILE_DIR:Game>/menus"
        COMMENT "Copying menu assets to build directory"
    )
    message(STATUS "Menus directory found: ${MENUS_SOURCE_DIR}")
endif()

# Copy character data (items, loadouts, attachment rules)
set(CHARACTER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/character")
if(EXISTS "${CHARACTER_SOURCE_DIR}")
    add_custom_command(TARGET Game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Game>/character"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CHARACTER_SOURCE_DIR}" "$<TARGET_FILE_DIR:Game>/character"
        COMMENT "Copying character data to build directory"
    )
    message(STATUS "Character directory found: ${CHARACTER_SOURCE_DIR}")
endif()

# Legacy menu folder (optional)
set(MENU_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../menu")
if(EXISTS "${MENU_SOURCE_DIR}")
    add_custom_command(TARGET Game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Game>/menu"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${MENU_SOURCE_DIR}" "$<TARGET_FILE_DIR:Game>/menu"
        COMMENT "Copying legacy menu assets to build directory"
    )
endif()

add_subdirectory(server)
