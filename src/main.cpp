#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include "app/app.hpp"
#include "chart/simfile_parser.hpp"
#include "chart/song_library.hpp"
#include "data/config.hpp"
#include "data/config_loader.hpp"
#include "data/data_paths.hpp"
#include "data/high_scores.hpp"
#include "gameplay/gameplay_view.hpp"
#include "render/gl_quad_renderer.hpp"
#include "screens/attract_screen.hpp"
#include "screens/calibration_screen.hpp"
#include "screens/gameplay_screen.hpp"
#include "screens/input_remap_screen.hpp"
#include "screens/play_request.hpp"
#include "screens/results.hpp"
#include "screens/results_screen.hpp"
#include "screens/select_screen.hpp"
#include "screens/screen_manager.hpp"
#include "screens/title_screen.hpp"

namespace fs = std::filesystem;

namespace {

void print_help() {
    std::cout << "Usage: tundra-dance [options]\n"
              << "  --headless              Run without window/GL context\n"
              << "  --smoke-test [N]        Run N frames and exit cleanly (default: 10)\n"
              << "  --no-vsync              Disable vertical sync\n"
              << "  --attract-timeout <s>   Idle seconds before Attract (default 30; <=0 disables)\n"
              << "  --songs <dir>           Songs folder to scan (default: ./songs, ./data/songs)\n"
              << "  --start-screen <name>   Start on 'title' or 'select' (default: title)\n"
              << "  --data-dir <path>       Override the data directory (config.json/scores.json)\n"
              << "  --xdg                   Use the Linux XDG data directory instead of ./data\n"
              << "                          (also enabled by the TUNDRA_XDG=1 environment variable)\n"
              << "  --gameplay-demo <file>  TEMPORARY: render a simfile's first chart (.sm/.ssc)\n"
              << "  --speed <mod>           Speed mod for the demo: Nx / Xn, cN, or mN (default 1x)\n"
              << "  --downscroll            Mirror the demo field for downscroll\n"
              << "  --fail-off              Fail-Off: demo keeps playing at zero life\n"
              << "  --help, -h              Show this help\n";
}

} // namespace

int main(int argc, char* argv[]) {
    std::cout << "Tundra Dance - 4-Panel Rhythm Game Engine v0.1.0\n";

    td::AppConfig config;
    config.window.title = "Tundra Dance";
    config.window.width = 1280;
    config.window.height = 720;
    config.window.vsync = true;

    // TEMPORARY: `--gameplay-demo` is a throwaway harness so the real GL note-field
    // path and the live music clock can be exercised before C1's screen manager.
    std::string demo_path;
    std::string speed_text;
    bool downscroll = false;
    bool fail_off = false;
    double attract_timeout = 30.0;
    bool attract_timeout_given = false;
    bool xdg_flag = false;
    std::string data_dir_text;
    std::string songs_dir_text;
    std::string start_screen_text;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--headless") {
            config.window.headless = true;
        } else if (arg == "--smoke-test") {
            int frames = 10;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                frames = std::stoi(argv[++i]);
            }
            config.smoke_test_frames = frames;
        } else if (arg == "--no-vsync") {
            config.window.vsync = false;
        } else if (arg == "--attract-timeout") {
            attract_timeout_given = true;
            if (i + 1 < argc) {
                const std::string value = argv[++i];
                try {
                    attract_timeout = std::stod(value);
                } catch (const std::exception&) {
                    std::cerr << "[main] Invalid --attract-timeout '" << value
                              << "'; using default 30s\n";
                }
            } else {
                std::cerr << "[main] --attract-timeout requires a number of seconds\n";
            }
        } else if (arg == "--gameplay-demo") {
            if (i + 1 < argc) {
                demo_path = argv[++i];
            } else {
                std::cerr << "[main] --gameplay-demo requires a simfile path\n";
            }
        } else if (arg == "--speed") {
            if (i + 1 < argc) {
                speed_text = argv[++i];
            } else {
                std::cerr << "[main] --speed requires a mod value (e.g. C400, 1.5x, M600)\n";
            }
        } else if (arg == "--downscroll") {
            downscroll = true;
        } else if (arg == "--fail-off") {
            fail_off = true;
        } else if (arg == "--xdg") {
            xdg_flag = true;
        } else if (arg == "--songs") {
            if (i + 1 < argc) {
                songs_dir_text = argv[++i];
            } else {
                std::cerr << "[main] --songs requires a directory path\n";
            }
        } else if (arg == "--start-screen") {
            if (i + 1 < argc) {
                start_screen_text = argv[++i];
            } else {
                std::cerr << "[main] --start-screen requires 'title' or 'select'\n";
            }
        } else if (arg == "--data-dir") {
            if (i + 1 < argc) {
                data_dir_text = argv[++i];
            } else {
                std::cerr << "[main] --data-dir requires a path; ignoring the flag and using "
                             "the default data directory\n";
            }
        } else if (arg == "--help" || arg == "-h") {
            print_help();
            return 0;
        }
    }

    if (!demo_path.empty() && attract_timeout_given) {
        std::cerr << "[main] --attract-timeout is ignored with --gameplay-demo\n";
    }

    // C2: resolve the local data directory and restore config/high scores before
    // the window is created (video settings must be applied to AppConfig first).
    const char* xdg_env = std::getenv("TUNDRA_XDG");
    const bool prefer_xdg = xdg_flag || (xdg_env != nullptr && std::string(xdg_env) == "1");
    const char* xdg_home_env = std::getenv("XDG_DATA_HOME");
    const char* home_env = std::getenv("HOME");
    const td::ResolvedDataPaths paths = td::resolve_data_paths(
        td::default_executable_dir(), prefer_xdg, xdg_home_env != nullptr ? xdg_home_env : "",
        home_env != nullptr ? home_env : "",
        data_dir_text.empty() ? fs::path{} : fs::path(data_dir_text));

    std::string config_message;
    td::ConfigLoadStatus config_status = td::ConfigLoadStatus::UsedDefaults;
    td::GameConfig game_config = td::load_config(paths.config_file, &config_message, &config_status);
    if (!config_message.empty()) {
        if (config_status == td::ConfigLoadStatus::UsedDefaults) {
            std::cerr << config_message << "\n";
        } else {
            std::cout << config_message << "\n";
        }
    }
    config.window.width = game_config.video.width;
    config.window.height = game_config.video.height;
    config.window.vsync = game_config.video.vsync;
    config.window.fullscreen = game_config.video.fullscreen;

    std::string scores_message;
    td::ScoresLoadStatus scores_status = td::ScoresLoadStatus::UsedDefaults;
    td::HighScores high_scores =
        td::load_high_scores(paths.scores_file, &scores_message, &scores_status);
    if (!scores_message.empty()) {
        if (scores_status == td::ScoresLoadStatus::UsedDefaults) {
            std::cerr << scores_message << "\n";
        } else {
            std::cout << scores_message << "\n";
        }
    }

    // C3: resolve and scan the songs directory. Non-fatal: a missing/empty
    // library is a valid wheel with zero entries.
    td::SongLibrary library;
    {
        fs::path songs_dir;
        if (!songs_dir_text.empty()) {
            songs_dir = fs::path(songs_dir_text);
        } else {
            const fs::path exe_dir = td::default_executable_dir();
            const fs::path candidates[] = {fs::path("songs"), fs::path("data") / "songs",
                                           exe_dir / "songs"};
            for (const fs::path& candidate : candidates) {
                std::error_code ec;
                if (fs::is_directory(candidate, ec)) {
                    songs_dir = candidate;
                    break;
                }
            }
        }
        if (!songs_dir.empty()) {
            library.scan_directory(songs_dir);
            std::cout << "[SongLibrary] scanned '" << songs_dir.string() << "': "
                      << library.total_songs() << " songs, " << library.total_charts()
                      << " charts\n";
        } else {
            std::cout << "[SongLibrary] no songs directory found; wheel will be empty\n";
        }
    }

    td::PlayRequest play_request;
    td::ResultsSummary results_summary;

    td::App app(config);
    if (!app.init()) {
        std::cerr << "Failed to initialize application.\n";
        return 1;
    }

    // C6: honor a saved remap from boot. Safe when the config was missing (the
    // defaults are applied). Reserved Escape/pad-Back are always re-installed.
    app.input_manager().apply_bindings(game_config.input);

    td::GlQuadRenderer quad_renderer;
    td::GameplayView gameplay;
    std::unique_ptr<td::ScreenManager> shell;

    if (!demo_path.empty()) {
        td::SimfileParser parser;
        if (!parser.parse_file(demo_path)) {
            std::cerr << "[main] Failed to parse simfile: " << demo_path << "\n";
            return 1;
        }
        if (parser.charts().empty()) {
            std::cerr << "[main] Simfile has no supported 4-panel charts: " << demo_path << "\n";
            return 1;
        }

        std::string audio_path;
        if (!parser.metadata().music_path.empty()) {
            const fs::path resolved = fs::path(demo_path).parent_path() / parser.metadata().music_path;
            if (fs::exists(resolved)) {
                audio_path = resolved.string();
            } else {
                std::cerr << "[main] Music file not found: " << resolved.string() << "\n";
            }
        }

        td::GameplayOptions options;
        if (!speed_text.empty()) {
            if (!td::parse_speed_mod(speed_text, options.speed)) {
                std::cerr << "[main] Invalid --speed '" << speed_text
                          << "'; defaulting to X-mod 1x\n";
                options.speed = td::SpeedMod{};
            }
        }
        options.scroll = downscroll ? td::ScrollDirection::Down : td::ScrollDirection::Up;
        options.fail_enabled = !fail_off;

        if (!gameplay.init(parser.charts().front(), app.judgment_constants(), audio_path, options)) {
            std::cerr << "[main] Failed to initialize gameplay demo\n";
            return 1;
        }

        if (!app.window().is_headless()) {
            if (!quad_renderer.init()) {
                std::cerr << "[main] Failed to initialize quad renderer\n";
            }
        }

        app.set_update_callback([&gameplay, &app](double fixed_dt) {
            auto events = app.input_manager().poll_events();
            gameplay.handle_input_events(events, app.input_reference_ns());
            const std::array<bool, 4> held = {
                app.input_manager().is_action_down(td::GameAction::Left),
                app.input_manager().is_action_down(td::GameAction::Down),
                app.input_manager().is_action_down(td::GameAction::Up),
                app.input_manager().is_action_down(td::GameAction::Right),
            };
            gameplay.update(fixed_dt, held);
        });
        app.set_render_callback([&gameplay, &quad_renderer, &app](double /*alpha*/) {
            if (quad_renderer.is_initialized()) {
                quad_renderer.begin(app.window().width(), app.window().height());
            }
            gameplay.render(quad_renderer, app.window().width(), app.window().height());
            if (quad_renderer.is_initialized()) {
                quad_renderer.end();
            }
        });
    } else {
        // C1 default path: the arcade shell state machine (Title -> Attract -> Select).
        shell = std::make_unique<td::ScreenManager>(attract_timeout);
        shell->add_screen(std::make_unique<td::TitleScreen>());
        shell->add_screen(std::make_unique<td::AttractScreen>());
        shell->add_screen(std::make_unique<td::SelectScreen>());
        shell->add_screen(std::make_unique<td::GameplayScreen>());
        shell->add_screen(std::make_unique<td::ResultsScreen>());
        shell->add_screen(
            std::make_unique<td::CalibrationScreen>(paths.data_dir / "calibration_click.wav"));
        shell->add_screen(std::make_unique<td::InputRemapScreen>());
        shell->context().config = &game_config;
        shell->context().scores = &high_scores;
        shell->context().library = &library;
        shell->context().constants = &app.judgment_constants();
        shell->context().play_request = &play_request;
        shell->context().results = &results_summary;
        shell->context().action_down = [&app](td::GameAction action) {
            return app.input_manager().is_action_down(action);
        };
        shell->context().input = &app.input_manager();

        td::ScreenId start_screen = td::ScreenId::Title;
        if (!start_screen_text.empty()) {
            if (start_screen_text == "select") {
                start_screen = td::ScreenId::Select;
            } else if (start_screen_text != "title") {
                std::cerr << "[main] Unknown --start-screen '" << start_screen_text
                          << "'; using title\n";
            }
        }
        shell->start(start_screen);

        if (!app.window().is_headless()) {
            if (!quad_renderer.init()) {
                std::cerr << "[main] Failed to initialize quad renderer\n";
            }
        }

        app.set_update_callback([&shell, &app](double fixed_dt) {
            auto events = app.input_manager().poll_events();
            shell->context().input_reference_ns = app.input_reference_ns();
            shell->update(fixed_dt, events);
        });
        app.set_render_callback([&shell, &quad_renderer, &app](double /*alpha*/) {
            if (quad_renderer.is_initialized()) {
                quad_renderer.begin(app.window().width(), app.window().height());
            }
            shell->render(quad_renderer, app.window().width(), app.window().height());
            if (quad_renderer.is_initialized()) {
                quad_renderer.end();
            }
        });
        // Back quits when the active screen does not consume it as navigation; the
        // App already filters the raw key through InputManager's Back mapping.
        app.set_event_callback([&shell](const SDL_Event& /*event*/) -> bool {
            return shell->back_navigates();
        });
    }

    app.run();

    // C2: persist config and high scores on a clean exit. Both saves are atomic
    // and idempotent; failures are warnings, never fatal.
    std::string save_message;
    if (!td::save_config(paths.config_file, game_config, &save_message) &&
        !save_message.empty()) {
        std::cerr << save_message << "\n";
    }
    if (!td::save_high_scores(paths.scores_file, high_scores, &save_message) &&
        !save_message.empty()) {
        std::cerr << save_message << "\n";
    }

    gameplay.shutdown();
    quad_renderer.shutdown();
    std::cout << "Tundra Dance shut down cleanly.\n";
    return 0;
}
