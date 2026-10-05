#pragma once

#include <string>
#include <SDL3/SDL.h>
#include <glad/glad.h>

namespace blaze4k {

struct WindowConfig {
    std::string title = "Blaze 4k";
    int width = 1280;
    int height = 720;
    bool vsync = true;
    bool fullscreen = false;
    bool resizable = true;
    bool headless = false;
};

class Window {
public:
    explicit Window(const WindowConfig& config = WindowConfig{});
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    bool init();
    void shutdown();

    void swap_buffers();
    void on_resize(int new_width, int new_height);

    // #112: show/hide the OS mouse cursor (SDL3, global to the SDL mouse, so it
    // holds in windowed and fullscreen). A no-op without an SDL window
    // (headless / before init / after shutdown). Idempotent.
    void set_cursor_visible(bool visible);

    [[nodiscard]] SDL_Window* handle() const { return window_; }
    [[nodiscard]] SDL_GLContext gl_context() const { return gl_context_; }
    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }
    [[nodiscard]] bool is_initialized() const { return is_initialized_; }
    [[nodiscard]] bool is_headless() const { return config_.headless; }
    [[nodiscard]] bool cursor_hidden() const { return cursor_hidden_; }

private:
    WindowConfig config_;
    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    bool is_initialized_ = false;
    bool cursor_hidden_ = false; // #112: what this wrapper last asked SDL for
};

} // namespace blaze4k
