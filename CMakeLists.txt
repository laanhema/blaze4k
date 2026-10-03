cmake_minimum_required(VERSION 3.24)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
project(blaze-4k VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(FetchContent)

# 1. SDL3
set(SDL_SHARED OFF CACHE BOOL "Build SDL static library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build SDL static library" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build SDL test library" FORCE)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.8
    GIT_SHALLOW TRUE
)

# 2. GLAD (OpenGL 3.3 Core)
set(GLAD_PROFILE "core" CACHE STRING "GLAD OpenGL profile" FORCE)
set(GLAD_API "gl=3.3" CACHE STRING "GLAD API" FORCE)
set(GLAD_GENERATOR "c" CACHE STRING "GLAD Language" FORCE)
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v0.1.36
    GIT_SHALLOW TRUE
)

# 3. nlohmann/json
set(JSON_BuildTests OFF CACHE BOOL "Build JSON tests" FORCE)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)

# 4. miniaudio
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.21
    GIT_SHALLOW TRUE
)

# 5. stb (stb_truetype, etc.)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(SDL3 glad nlohmann_json miniaudio stb)

# Create interface target for miniaudio
if(NOT TARGET miniaudio)
    add_library(miniaudio INTERFACE)
    target_include_directories(miniaudio INTERFACE ${miniaudio_SOURCE_DIR})
endif()

# Create interface target for stb
if(NOT TARGET stb)
    add_library(stb INTERFACE)
    target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
endif()

# Core library target
add_library(blaze4k_core STATIC
    src/app/window.cpp
    src/app/app.cpp
    src/audio/miniaudio_impl.cpp
    src/audio/audio_engine.cpp
    src/audio/sound_stream.cpp
    src/input/input_manager.cpp
    src/chart/msd_file.cpp
    src/chart/timing_data.cpp
    src/chart/simfile_parser.cpp
    src/chart/note_parser.cpp
    src/chart/song_library.cpp
    src/timing/music_clock.cpp
    src/timing/judgment_constants.cpp
    src/timing/offset_calibration.cpp
    src/render/texture.cpp
    src/render/stb_image_impl.cpp
    src/render/texture_cache.cpp
    src/render/gl_quad_renderer.cpp
    src/render/background_renderer.cpp
    src/render/bitmap_font.cpp
    src/render/unicode_text.cpp
    src/render/note_art.cpp
    src/render/theme_textures.cpp
    src/render/stb_truetype_impl.cpp
    src/render/ttf_font.cpp
    src/audio/preview_player.cpp
    src/audio/metronome.cpp
    src/audio/ui_sounds.cpp
    src/audio/assist_tick_player.cpp
    src/gameplay/speed_mod.cpp
    src/gameplay/gameplay_options.cpp
    src/gameplay/assist_tick_schedule.cpp
    src/gameplay/note_field.cpp
    src/gameplay/judgment_engine.cpp
    src/gameplay/score_keeper.cpp
    src/gameplay/hud_renderer.cpp
    src/gameplay/judgment_animator.cpp
    src/gameplay/life_keeper.cpp
    src/gameplay/noteskin.cpp
    src/gameplay/note_field_renderer.cpp
    src/gameplay/gameplay_view.cpp
    src/data/judgment_constants_loader.cpp
    src/data/config_loader.cpp
    src/data/high_scores.cpp
    src/data/data_paths.cpp
    src/screens/screen_manager.cpp
    src/screens/title_screen.cpp
    src/screens/attract_screen.cpp
    src/screens/select_screen.cpp
    src/screens/options_menu.cpp
    src/screens/gameplay_screen.cpp
    src/screens/results.cpp
    src/screens/results_screen.cpp
    src/screens/song_display_text.cpp
    src/screens/results_anim.cpp
    src/screens/calibration_screen.cpp
    src/screens/input_remap.cpp
    src/screens/input_remap_screen.cpp
    src/screens/select_placeholder_screen.cpp
)

target_include_directories(blaze4k_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(blaze4k_core PUBLIC
    SDL3::SDL3-static
    glad
    nlohmann_json::nlohmann_json
    miniaudio
    stb
    ${CMAKE_DL_LIBS}
)

# Main executable
add_executable(blaze-4k
    src/main.cpp
)

target_link_libraries(blaze-4k PRIVATE
    blaze4k_core
)

# Copy bundled assets (fallback background, judgment constants) next to the
# binary so a fresh `build/` run finds them via the runtime candidate paths.
add_custom_command(TARGET blaze-4k POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:blaze-4k>/assets
    COMMENT "Copying bundled assets next to blaze-4k"
)

# Testing
enable_testing()
add_subdirectory(tests)
