add_executable(sanity_test
    sanity_test.cpp
)

target_link_libraries(sanity_test PRIVATE
    nlohmann_json::nlohmann_json
    stb
)

add_test(NAME sanity_test COMMAND sanity_test)

add_executable(app_test
    app_test.cpp
)

target_link_libraries(app_test PRIVATE
    tundra_core
)

add_test(NAME app_test COMMAND app_test)

add_executable(audio_test
    audio_test.cpp
)

target_link_libraries(audio_test PRIVATE
    tundra_core
)

add_test(NAME audio_test COMMAND audio_test)

add_executable(input_test
    input_test.cpp
)

target_link_libraries(input_test PRIVATE
    tundra_core
)

add_test(NAME input_test COMMAND input_test)

add_executable(parser_test
    parser_test.cpp
)

target_link_libraries(parser_test PRIVATE
    tundra_core
)

add_test(NAME parser_test COMMAND parser_test)

add_executable(note_parser_test
    note_parser_test.cpp
)

target_link_libraries(note_parser_test PRIVATE
    tundra_core
)

add_test(NAME note_parser_test COMMAND note_parser_test)
