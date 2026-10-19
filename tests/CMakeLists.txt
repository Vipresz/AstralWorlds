include(FetchContent)

include("${CMAKE_SOURCE_DIR}/cmake/Sqlite3Amalgamation.cmake")
add_sqlite3_amalgamation()

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

file(GLOB GAME_CHARACTER_SOURCES
    "${CMAKE_SOURCE_DIR}/src/character/*.cpp"
)
list(FILTER GAME_CHARACTER_SOURCES EXCLUDE REGEX ".*/Character(Nameplate|Paperdoll|SpeechBubble)Renderer\\.cpp$")
file(GLOB GAME_CHARACTER_SYSTEM_SOURCES
    "${CMAKE_SOURCE_DIR}/src/systems/character/*.cpp"
)

set(GAME_TEST_SOURCES
    BackendGameLogicTest.cpp
    CharacterBodyAssetCacheTest.cpp
    CharacterInventoryDatabaseTest.cpp
    CharacterLoadE2ETest.cpp
    ChatInputFocusTest.cpp
    GameMenusManifestTest.cpp
    GameNetworkE2ETest.cpp
    GameTestMain.cpp
    SkillBackendTest.cpp
    "${CMAKE_SOURCE_DIR}/src/AppPaths.cpp"
    "${CMAKE_SOURCE_DIR}/src/GameNetworkConfig.cpp"
    "${CMAKE_SOURCE_DIR}/src/WorldSceneBootstrap.cpp"
    "${CMAKE_SOURCE_DIR}/src/menus/src/ManifestTradeConfig.cpp"
    ${GAME_CHARACTER_SOURCES}
    ${GAME_CHARACTER_SYSTEM_SOURCES}
)

add_executable(GameTests ${GAME_TEST_SOURCES})

target_include_directories(GameTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/menus/src
    ${CMAKE_SOURCE_DIR}/src/menus/main_menu/src
    ${CMAKE_SOURCE_DIR}/RenderingEngine/tests
    ${CMAKE_SOURCE_DIR}/RenderingEngine
    ${CMAKE_SOURCE_DIR}/RenderingEngine/platform
)

target_compile_definitions(GameTests PRIVATE
    GAME_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(GameTests PRIVATE
    Engine
    Platform
    sqlite3
    GTest::gtest
)

if(DEFINED NUKLEAR_DIR)
    target_include_directories(GameTests PRIVATE ${NUKLEAR_DIR} ${NUKLEAR_DIR}/demo/glfw_opengl3)
    target_compile_definitions(GameTests PRIVATE PLATFORM_HAS_NUKLEAR)
endif()

target_compile_features(GameTests PRIVATE cxx_std_17)

set_target_properties(GameTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

function(game_tests_copy_tree target source_dir dest_name)
    if(EXISTS "${source_dir}")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/${dest_name}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${source_dir}" "$<TARGET_FILE_DIR:${target}>/${dest_name}"
            COMMENT "Copying ${dest_name} assets for GameTests"
        )
    endif()
endfunction()

game_tests_copy_tree(GameTests "${CMAKE_SOURCE_DIR}/graphics" "graphics")
game_tests_copy_tree(GameTests "${CMAKE_SOURCE_DIR}/src/character" "character")
game_tests_copy_tree(GameTests "${CMAKE_SOURCE_DIR}/src/menus" "menus")
game_tests_copy_tree(GameTests "${CMAKE_SOURCE_DIR}/tests/fixtures" "tests/fixtures")

if(EXISTS "${CMAKE_SOURCE_DIR}/RenderingEngine/config/join_load_stress_scale.json")
    add_custom_command(TARGET GameTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:GameTests>/config"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/RenderingEngine/config/join_load_stress_scale.json"
            "$<TARGET_FILE_DIR:GameTests>/config/join_load_stress_scale.json"
        COMMENT "Copying join load stress scale config for GameTests"
    )
endif()

set(GLEW_DLL_DIR "${CMAKE_SOURCE_DIR}/../game_libs/glew-2.1.0/bin/Release/x64")
if(EXISTS "${GLEW_DLL_DIR}/glew32.dll")
    add_custom_command(TARGET GameTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${GLEW_DLL_DIR}/glew32.dll"
        "$<TARGET_FILE_DIR:GameTests>"
        COMMENT "Copying GLEW DLL for GameTests"
    )
endif()

include(GoogleTest)
gtest_discover_tests(GameTests DISCOVERY_MODE PRE_TEST)

add_test(NAME GameTests COMMAND GameTests)
set_tests_properties(GameTests PROPERTIES LABELS "game;e2e")
