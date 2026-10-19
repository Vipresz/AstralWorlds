# Dedicated multiplayer relay server (no rendering window)

include("${CMAKE_SOURCE_DIR}/cmake/Sqlite3Amalgamation.cmake")
add_sqlite3_amalgamation()

add_executable(GameServer
    main.cpp
    ../AppPaths.cpp
    ../character/CharacterDatabase.cpp
    ../character/CharacterInventoryDatabase.cpp
    ../character/CharacterTypes.cpp
    ../character/GameItemCatalog.cpp
    ../menus/src/ManifestTradeConfig.cpp
)

target_include_directories(GameServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../menus/src
)

target_link_libraries(GameServer PRIVATE
    Engine
    Platform
    sqlite3
)

set_target_properties(GameServer PROPERTIES
    OUTPUT_NAME "game_server"
)

set_target_properties(GameServer PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

# Character stats / loadout data used by authoritative movement.
set(CHARACTER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../character")
if(EXISTS "${CHARACTER_SOURCE_DIR}")
    add_custom_command(TARGET GameServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:GameServer>/character"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CHARACTER_SOURCE_DIR}" "$<TARGET_FILE_DIR:GameServer>/character"
        COMMENT "Copying character data to GameServer output directory"
    )
endif()

set(MENUS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../menus")
if(EXISTS "${MENUS_SOURCE_DIR}")
    add_custom_command(TARGET GameServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:GameServer>/menus"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${MENUS_SOURCE_DIR}" "$<TARGET_FILE_DIR:GameServer>/menus"
        COMMENT "Copying menus data to GameServer output directory"
    )
endif()
