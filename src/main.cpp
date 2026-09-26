#include <iostream>
#include <string>
#include <vector>
#include "app/app.hpp"



int main(int argc, char* argv[]) {
    std::cout << "Tundra Dance - 4-Panel Rhythm Game Engine v0.1.0\n";

    td::AppConfig config;
    config.window.title = "Tundra Dance";
    config.window.width = 1280;
    config.window.height = 720;
    config.window.vsync = true;

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
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: tundra-dance [options]\n"
                      << "  --headless         Run without window/GL context\n"
                      << "  --smoke-test [N]   Run N frames and exit cleanly (default: 10)\n"
                      << "  --no-vsync         Disable vertical sync\n"
                      << "  --help, -h         Show this help\n";
            return 0;
        }
    }

    td::App app(config);
    if (!app.init()) {
        std::cerr << "Failed to initialize application.\n";
        return 1;
    }

    app.run();
    std::cout << "Tundra Dance shut down cleanly.\n";
    return 0;
}
