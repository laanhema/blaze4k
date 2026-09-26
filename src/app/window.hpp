#pragma once

#include <string>
#include <SDL3/SDL.h>
#include <glad/glad.h>

namespace td {

struct WindowConfig {
    std::string title = "Tundra Dance";
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

    [[nodiscard]] SDL_Window* handle() const { return window_; }
    [[nodiscard]] SDL_GLContext gl_context() const { return gl_context_; }
    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }
    [[nodiscard]] bool is_initialized() const { return is_initialized_; }
    [[nodiscard]] bool is_headless() const { return config_.headless; }

private:
    WindowConfig config_;
    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    bool is_initialized_ = false;
};

} // namespace td
