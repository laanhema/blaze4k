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


add_executable(judgment_engine_test
    judgment_engine_test.cpp
)

target_link_libraries(judgment_engine_test PRIVATE
    tundra_core
)

add_test(NAME judgment_engine_test COMMAND judgment_engine_test)


add_executable(score_keeper_test
    score_keeper_test.cpp
)

target_link_libraries(score_keeper_test PRIVATE
    tundra_core
)

add_test(NAME score_keeper_test COMMAND score_keeper_test)


add_executable(life_keeper_test
    life_keeper_test.cpp
)

target_link_libraries(life_keeper_test PRIVATE
    tundra_core
)

add_test(NAME life_keeper_test COMMAND life_keeper_test)

add_executable(metronome_sync_test
    metronome_sync_test.cpp
)

target_link_libraries(metronome_sync_test PRIVATE
    tundra_core
)

add_test(NAME metronome_sync_test COMMAND metronome_sync_test)

add_executable(screen_manager_test
    screen_manager_test.cpp
)

target_link_libraries(screen_manager_test PRIVATE
    tundra_core
)

add_test(NAME screen_manager_test COMMAND screen_manager_test)

add_executable(config_persistence_test
    config_persistence_test.cpp
)

target_link_libraries(config_persistence_test PRIVATE
    tundra_core
)

add_test(NAME config_persistence_test COMMAND config_persistence_test)

add_executable(select_screen_test
    select_screen_test.cpp
)

target_link_libraries(select_screen_test PRIVATE
    tundra_core
)

add_test(NAME select_screen_test COMMAND select_screen_test)

add_executable(preview_player_test
    preview_player_test.cpp
)

target_link_libraries(preview_player_test PRIVATE
    tundra_core
)

add_test(NAME preview_player_test COMMAND preview_player_test)

add_executable(texture_test
    texture_test.cpp
)

target_link_libraries(texture_test PRIVATE
    tundra_core
)

add_test(NAME texture_test COMMAND texture_test)
