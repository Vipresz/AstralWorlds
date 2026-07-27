# Dedicated multiplayer relay server (no rendering window)

add_executable(GameServer main.cpp)

target_include_directories(GameServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(GameServer PRIVATE
    Engine
    Platform
)

set_target_properties(GameServer PROPERTIES
    OUTPUT_NAME "game_server"
)

set_target_properties(GameServer PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
