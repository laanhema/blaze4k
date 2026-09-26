#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <exception>
#include <filesystem>
#include "app/app.hpp"
#include "chart/simfile_parser.hpp"
#include "gameplay/gameplay_view.hpp"
#include "render/gl_quad_renderer.hpp"
#include "screens/attract_screen.hpp"
#include "screens/select_placeholder_screen.hpp"
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
        } else if (arg == "--help" || arg == "-h") {
            print_help();
            return 0;
        }
    }

    if (!demo_path.empty() && attract_timeout_given) {
        std::cerr << "[main] --attract-timeout is ignored with --gameplay-demo\n";
    }

    td::App app(config);
    if (!app.init()) {
        std::cerr << "Failed to initialize application.\n";
        return 1;
    }

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
        shell->add_screen(std::make_unique<td::SelectPlaceholderScreen>());
        shell->start(td::ScreenId::Title);

        if (!app.window().is_headless()) {
            if (!quad_renderer.init()) {
                std::cerr << "[main] Failed to initialize quad renderer\n";
            }
        }

        app.set_update_callback([&shell, &app](double fixed_dt) {
            auto events = app.input_manager().poll_events();
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

    gameplay.shutdown();
    quad_renderer.shutdown();
    std::cout << "Tundra Dance shut down cleanly.\n";
    return 0;
}
