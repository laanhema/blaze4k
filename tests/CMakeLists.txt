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

add_executable(song_library_test
    song_library_test.cpp
)

target_link_libraries(song_library_test PRIVATE
    tundra_core
)

add_test(NAME song_library_test COMMAND song_library_test)

add_executable(parser_hardening_test
    parser_hardening_test.cpp
)

target_link_libraries(parser_hardening_test PRIVATE
    tundra_core
)

add_test(NAME parser_hardening_test COMMAND parser_hardening_test)

add_executable(music_clock_test
    music_clock_test.cpp
)

target_link_libraries(music_clock_test PRIVATE
    tundra_core
)

add_test(NAME music_clock_test COMMAND music_clock_test)

add_executable(judgment_constants_test
    judgment_constants_test.cpp
)

target_link_libraries(judgment_constants_test PRIVATE
    tundra_core
)

add_test(NAME judgment_constants_test COMMAND judgment_constants_test)


add_executable(note_field_test
    note_field_test.cpp
)

target_link_libraries(note_field_test PRIVATE
    tundra_core
)

add_test(NAME note_field_test COMMAND note_field_test)
