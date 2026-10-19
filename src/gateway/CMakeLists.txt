add_executable(Gateway
    main.cpp
)

target_link_libraries(Gateway PRIVATE
    Engine
    Platform
)

set_target_properties(Gateway PROPERTIES
    OUTPUT_NAME "gateway"
)
