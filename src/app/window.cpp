#include "app/window.hpp"
#include <iostream>
#include <cstdlib>

namespace td {

Window::Window(const WindowConfig& config)
    : config_(config), width_(config.width), height_(config.height) {}

Window::~Window() {
    shutdown();
}

Window::Window(Window&& other) noexcept
    : config_(other.config_),
      window_(other.window_),
      gl_context_(other.gl_context_),
      width_(other.width_),
      height_(other.height_),
      is_initialized_(other.is_initialized_) {
    other.window_ = nullptr;
    other.gl_context_ = nullptr;
    other.is_initialized_ = false;
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        shutdown();
        config_ = other.config_;
        window_ = other.window_;
        gl_context_ = other.gl_context_;
        width_ = other.width_;
        height_ = other.height_;
        is_initialized_ = other.is_initialized_;

        other.window_ = nullptr;
        other.gl_context_ = nullptr;
        other.is_initialized_ = false;
    }
    return *this;
}

bool Window::init() {
    if (is_initialized_) {
        return true;
    }

    if (config_.headless) {
        std::cout << "[Window] Running in headless mode (no display/GL context).\n";
        is_initialized_ = true;
        return true;
    }

    // Auto-detect missing display server on Linux
#if defined(__linux__)
    const char* display = std::getenv("DISPLAY");
    const char* wayland = std::getenv("WAYLAND_DISPLAY");
    if (!display && !wayland) {
        std::cout << "[Window] No DISPLAY or WAYLAND_DISPLAY found; switching to headless mode.\n";
        config_.headless = true;
        is_initialized_ = true;
        return true;
    }
#endif

    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        std::cerr << "[Window] Failed to initialize SDL video: " << SDL_GetError() << "\n";
        return false;
    }

    // Request OpenGL 3.3 Core profile
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_WindowFlags flags = SDL_WINDOW_OPENGL;
    if (config_.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    window_ = SDL_CreateWindow(
        config_.title.c_str(),
        config_.width,
        config_.height,
        flags
    );

    if (!window_) {
        std::cerr << "[Window] Failed to create SDL window: " << SDL_GetError() << "\n";
        return false;
    }

    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
        std::cerr << "[Window] Failed to create OpenGL context: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        return false;
    }

    if (!SDL_GL_MakeCurrent(window_, gl_context_)) {
        std::cerr << "[Window] Failed to make GL context current: " << SDL_GetError() << "\n";
        shutdown();
        return false;
    }

    // Initialize glad
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
        std::cerr << "[Window] Failed to initialize GLAD loader\n";
        shutdown();
        return false;
    }

    // Set vsync
    SDL_GL_SetSwapInterval(config_.vsync ? 1 : 0);

    // Initial viewport
    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(window_, &w, &h);
    on_resize(w > 0 ? w : config_.width, h > 0 ? h : config_.height);

    std::cout << "[Window] OpenGL Context initialized: "
              << glGetString(GL_VERSION) << " ("
              << glGetString(GL_RENDERER) << ")\n";

    is_initialized_ = true;
    return true;
}

void Window::shutdown() {
    if (gl_context_) {
        SDL_GL_DestroyContext(gl_context_);
        gl_context_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    is_initialized_ = false;
}

void Window::swap_buffers() {
    if (window_ && gl_context_ && !config_.headless) {
        SDL_GL_SwapWindow(window_);
    }
}

void Window::on_resize(int new_width, int new_height) {
    width_ = new_width;
    height_ = new_height;
    if (is_initialized_ && !config_.headless) {
        glViewport(0, 0, width_, height_);
    }
}

} // namespace td
