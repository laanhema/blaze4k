#include "app/app.hpp"
#include "data/judgment_constants_loader.hpp"
#include <iostream>
#include <algorithm>

namespace td {

App::App(const AppConfig& config)
    : config_(config), window_(config.window) {}

App::~App() {
    stop();
    window_.shutdown();
    SDL_Quit();
}

bool App::init() {
    if (!window_.init()) {
        std::cerr << "[App] Failed to initialize window and graphics context.\n";
        return false;
    }

    perf_frequency_ = SDL_GetPerformanceFrequency();
    last_time_ = SDL_GetPerformanceCounter();
    accumulator_ = 0.0;
    frames_rendered_ = 0;
    is_running_ = true;

    std::string constants_message;
    ConstantsLoadStatus constants_status = ConstantsLoadStatus::UsedDefaults;
    judgment_constants_ = load_judgment_constants_from_candidates(
        {"data/judgment_constants.json", "assets/data/judgment_constants.json"},
        &constants_message, &constants_status);
    if (!constants_message.empty()) {
        if (constants_status == ConstantsLoadStatus::UsedDefaults) {
            std::cerr << constants_message << "\n";
        } else {
            std::cout << constants_message << "\n";
        }
    }

    std::cout << "[App] Initialized successfully (fixed_dt=" << config_.fixed_dt
              << "s, vsync=" << (config_.window.vsync ? "on" : "off") << ").\n";
    return true;
}

void App::stop() {
    is_running_ = false;
}

void App::run() {
    if (!is_running_) {
        if (!init()) {
            return;
        }
    }

    while (is_running_) {
        process_events();
        if (!is_running_) {
            break;
        }

        uint64_t current_time = SDL_GetPerformanceCounter();
        double frame_dt = static_cast<double>(current_time - last_time_) /
                          static_cast<double>(perf_frequency_);
        last_time_ = current_time;

        // Prevent accumulator spiral of death
        frame_dt = std::min(frame_dt, config_.max_frame_dt);
        accumulator_ += frame_dt;

        // Fixed-timestep physics/screen updates
        while (accumulator_ >= config_.fixed_dt) {
            on_update(config_.fixed_dt);
            accumulator_ -= config_.fixed_dt;
        }

        // Render pass with interpolation factor alpha
        double alpha = accumulator_ / config_.fixed_dt;
        on_render(alpha);

        window_.swap_buffers();
        frames_rendered_++;

        if (config_.smoke_test_frames > 0 && frames_rendered_ >= config_.smoke_test_frames) {
            std::cout << "[App] Smoke test finished (" << frames_rendered_ << " frames). Exiting cleanly.\n";
            stop();
        }
    }
}

void App::process_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            std::cout << "[App] Quit requested by OS.\n";
            stop();
        } else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            std::cout << "[App] Window close requested.\n";
            stop();
        } else if (event.type == SDL_EVENT_KEY_DOWN) {
            if (input_manager_.action_for_key(event.key.key) == GameAction::Back) {
                const bool consumed = event_cb_ && event_cb_(event);
                if (!consumed) {
                    std::cout << "[App] Back pressed. Exiting.\n";
                    stop();
                }
            }
        } else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            window_.on_resize(event.window.data1, event.window.data2);
        }

        input_manager_.handle_sdl_event(event);
        on_event(event);
    }
    // Same nanosecond timebase as InputEvent::timestamp_ns; used only to age
    // input events against the music clock, never as a gameplay clock.
    input_reference_ns_ = SDL_GetTicksNS();
}

void App::on_update(double fixed_dt) {
    if (update_cb_) {
        update_cb_(fixed_dt);
    }
}

void App::on_render(double alpha) {
    if (!window_.is_headless()) {
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    if (render_cb_) {
        render_cb_(alpha);
    }
}

void App::on_event(const SDL_Event& event) {
    (void)event;
}

} // namespace td
