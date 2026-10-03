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
    blaze4k_core
)

add_test(NAME app_test COMMAND app_test)

add_executable(audio_test
    audio_test.cpp
)

target_link_libraries(audio_test PRIVATE
    blaze4k_core
)

add_test(NAME audio_test COMMAND audio_test)

add_executable(input_test
    input_test.cpp
)

target_link_libraries(input_test PRIVATE
    blaze4k_core
)

add_test(NAME input_test COMMAND input_test)

add_executable(parser_test
    parser_test.cpp
)

target_link_libraries(parser_test PRIVATE
    blaze4k_core
)

add_test(NAME parser_test COMMAND parser_test)

add_executable(note_parser_test
    note_parser_test.cpp
)

target_link_libraries(note_parser_test PRIVATE
    blaze4k_core
)

add_test(NAME note_parser_test COMMAND note_parser_test)

add_executable(song_library_test
    song_library_test.cpp
)

target_link_libraries(song_library_test PRIVATE
    blaze4k_core
)

add_test(NAME song_library_test COMMAND song_library_test)

add_executable(parser_hardening_test
    parser_hardening_test.cpp
)

target_link_libraries(parser_hardening_test PRIVATE
    blaze4k_core
)

add_test(NAME parser_hardening_test COMMAND parser_hardening_test)

add_executable(music_clock_test
    music_clock_test.cpp
)

target_link_libraries(music_clock_test PRIVATE
    blaze4k_core
)

add_test(NAME music_clock_test COMMAND music_clock_test)

add_executable(judgment_constants_test
    judgment_constants_test.cpp
)

target_link_libraries(judgment_constants_test PRIVATE
    blaze4k_core
)

add_test(NAME judgment_constants_test COMMAND judgment_constants_test)


add_executable(note_field_test
    note_field_test.cpp
)

target_link_libraries(note_field_test PRIVATE
    blaze4k_core
)

add_test(NAME note_field_test COMMAND note_field_test)


add_executable(judgment_engine_test
    judgment_engine_test.cpp
)

target_link_libraries(judgment_engine_test PRIVATE
    blaze4k_core
)

add_test(NAME judgment_engine_test COMMAND judgment_engine_test)


add_executable(score_keeper_test
    score_keeper_test.cpp
)

target_link_libraries(score_keeper_test PRIVATE
    blaze4k_core
)

add_test(NAME score_keeper_test COMMAND score_keeper_test)


add_executable(hud_renderer_test
    hud_renderer_test.cpp
)

target_link_libraries(hud_renderer_test PRIVATE
    blaze4k_core
)

add_test(NAME hud_renderer_test COMMAND hud_renderer_test)


add_executable(life_keeper_test
    life_keeper_test.cpp
)

target_link_libraries(life_keeper_test PRIVATE
    blaze4k_core
)

add_test(NAME life_keeper_test COMMAND life_keeper_test)

add_executable(metronome_sync_test
    metronome_sync_test.cpp
)

target_link_libraries(metronome_sync_test PRIVATE
    blaze4k_core
)

add_test(NAME metronome_sync_test COMMAND metronome_sync_test)

add_executable(screen_manager_test
    screen_manager_test.cpp
)

target_link_libraries(screen_manager_test PRIVATE
    blaze4k_core
)

add_test(NAME screen_manager_test COMMAND screen_manager_test)

add_executable(config_persistence_test
    config_persistence_test.cpp
)

target_link_libraries(config_persistence_test PRIVATE
    blaze4k_core
)

add_test(NAME config_persistence_test COMMAND config_persistence_test)

add_executable(select_screen_test
    select_screen_test.cpp
)

target_link_libraries(select_screen_test PRIVATE
    blaze4k_core
)

add_test(NAME select_screen_test COMMAND select_screen_test)

add_executable(unicode_text_test
    unicode_text_test.cpp
)

target_link_libraries(unicode_text_test PRIVATE
    blaze4k_core
)

add_test(NAME unicode_text_test COMMAND unicode_text_test)

add_executable(bitmap_font_test
    bitmap_font_test.cpp
)

target_link_libraries(bitmap_font_test PRIVATE
    blaze4k_core
)

add_test(NAME bitmap_font_test COMMAND bitmap_font_test)

add_executable(options_menu_test
    options_menu_test.cpp
)

target_link_libraries(options_menu_test PRIVATE
    blaze4k_core
)

add_test(NAME options_menu_test COMMAND options_menu_test)

add_executable(offset_calibration_test
    offset_calibration_test.cpp
)

target_link_libraries(offset_calibration_test PRIVATE
    blaze4k_core
)

add_test(NAME offset_calibration_test COMMAND offset_calibration_test)

add_executable(calibration_screen_test
    calibration_screen_test.cpp
)

target_link_libraries(calibration_screen_test PRIVATE
    blaze4k_core
)

add_test(NAME calibration_screen_test COMMAND calibration_screen_test)

add_executable(preview_player_test
    preview_player_test.cpp
)

target_link_libraries(preview_player_test PRIVATE
    blaze4k_core
)

add_test(NAME preview_player_test COMMAND preview_player_test)

add_executable(texture_test
    texture_test.cpp
)

target_link_libraries(texture_test PRIVATE
    blaze4k_core
)

add_test(NAME texture_test COMMAND texture_test)

add_executable(background_test
    background_test.cpp
)

target_link_libraries(background_test PRIVATE
    blaze4k_core
)

# Locate the committed fallback asset from out-of-tree test runs (cwd is the
# tests build dir), mirroring main's executable-relative candidate.
target_compile_definitions(background_test PRIVATE
    BLAZE4K_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
)

add_test(NAME background_test COMMAND background_test)

add_executable(input_remap_test
    input_remap_test.cpp
)

target_link_libraries(input_remap_test PRIVATE
    blaze4k_core
)

add_test(NAME input_remap_test COMMAND input_remap_test)

add_executable(input_remap_screen_test
    input_remap_screen_test.cpp
)

target_link_libraries(input_remap_screen_test PRIVATE
    blaze4k_core
)

add_test(NAME input_remap_screen_test COMMAND input_remap_screen_test)

add_executable(results_test
    results_test.cpp
)

target_link_libraries(results_test PRIVATE
    blaze4k_core
)

add_test(NAME results_test COMMAND results_test)

add_executable(results_screen_test
    results_screen_test.cpp
)

target_link_libraries(results_screen_test PRIVATE
    blaze4k_core
)

add_test(NAME results_screen_test COMMAND results_screen_test)

add_executable(results_anim_test
    results_anim_test.cpp
)

target_link_libraries(results_anim_test PRIVATE
    blaze4k_core
)

add_test(NAME results_anim_test COMMAND results_anim_test)

add_executable(note_art_test
    note_art_test.cpp
)

target_link_libraries(note_art_test PRIVATE
    blaze4k_core
)

add_test(NAME note_art_test COMMAND note_art_test)

add_executable(noteskin_test
    noteskin_test.cpp
)

target_link_libraries(noteskin_test PRIVATE
    blaze4k_core
)

add_test(NAME noteskin_test COMMAND noteskin_test)

add_executable(note_field_renderer_test
    note_field_renderer_test.cpp
)

target_link_libraries(note_field_renderer_test PRIVATE
    blaze4k_core
)

add_test(NAME note_field_renderer_test COMMAND note_field_renderer_test)

add_executable(judgment_animator_test
    judgment_animator_test.cpp
)

target_link_libraries(judgment_animator_test PRIVATE
    blaze4k_core
)

add_test(NAME judgment_animator_test COMMAND judgment_animator_test)

add_executable(ui_sounds_test
    ui_sounds_test.cpp
)

target_link_libraries(ui_sounds_test PRIVATE
    blaze4k_core
)

add_test(NAME ui_sounds_test COMMAND ui_sounds_test)

add_executable(frame_stats_test
    frame_stats_test.cpp
)

target_link_libraries(frame_stats_test PRIVATE
    blaze4k_core
)

add_test(NAME frame_stats_test COMMAND frame_stats_test)

add_executable(perf_loop_test
    perf_loop_test.cpp
)

target_link_libraries(perf_loop_test PRIVATE
    blaze4k_core
)

# Locate the reference-pack chart from out-of-tree test runs (cwd is the tests
# build dir), mirroring background_test's BLAZE4K_ASSETS_DIR seam.
target_compile_definitions(perf_loop_test PRIVATE
    BLAZE4K_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME perf_loop_test COMMAND perf_loop_test)

add_executable(gameplay_screen_test
    gameplay_screen_test.cpp
)

target_link_libraries(gameplay_screen_test PRIVATE
    blaze4k_core
)

add_test(NAME gameplay_screen_test COMMAND gameplay_screen_test)

add_executable(assist_tick_test
    assist_tick_test.cpp
)

target_link_libraries(assist_tick_test PRIVATE
    blaze4k_core
)

add_test(NAME assist_tick_test COMMAND assist_tick_test)
