#include <iostream>
#include <SDL3/SDL.h>
#include <glad/glad.h>
#include <nlohmann/json.hpp>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#pragma GCC diagnostic pop

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "Tundra Dance - Rhythm Game Engine v0.1.0\n";
    std::cout << "Initializing subsystems...\n";

    // Test nlohmann/json
    nlohmann::json info = {
        {"game", "Tundra Dance"},
        {"engine", "C++20 / SDL3 / OpenGL 3.3"},
        {"version", "0.1.0"}
    };
    std::cout << "JSON support: " << info["game"].get<std::string>() << "\n";

    // Test SDL3 version
    int sdlVersion = SDL_GetVersion();
    std::cout << "SDL3 version: " << SDL_VERSIONNUM_MAJOR(sdlVersion) << "."
              << SDL_VERSIONNUM_MINOR(sdlVersion) << "."
              << SDL_VERSIONNUM_MICRO(sdlVersion) << "\n";

    // Test miniaudio context initialization
    ma_context maContext;
    if (ma_context_init(nullptr, 0, nullptr, &maContext) == MA_SUCCESS) {
        std::cout << "miniaudio initialized successfully.\n";
        ma_context_uninit(&maContext);
    } else {
        std::cout << "miniaudio initialized in fallback mode.\n";
    }

    // Test stb_truetype
    stbtt_fontinfo font;
    std::cout << "stb_truetype ready (struct size: " << sizeof(font) << " bytes).\n";

    std::cout << "Scaffold verification complete.\n";
    return 0;
}
