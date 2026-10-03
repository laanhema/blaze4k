#pragma once

#include <string>
#include <functional>
#include "app/frame_stats.hpp"
#include "app/window.hpp"
#include "gameplay/noteskin.hpp"
#include "input/input_manager.hpp"
#include "render/theme_textures.hpp"
#include "render/ttf_font.hpp"
#include "timing/judgment_constants.hpp"

namespace blaze4k {

struct AppConfig {
    WindowConfig window;
    double fixed_dt = 1.0 / 60.0;
    double max_frame_dt = 0.25; // Prevents accumulator spiral of death
    int smoke_test_frames = -1; // -1 = run until explicit quit

    // Presentation-only frame-time instrumentation (never the judgment path).
    // When `perf_report` is set, `run()` prints one FrameStats block on exit and
    // the PASS/FAIL verdict compares p99 against `perf_budget_ms`.
    bool perf_report = false;
    double perf_budget_ms = 1000.0 / 60.0; // 60 fps frame budget
};

class App {
public:
    explicit App(const AppConfig& config = AppConfig{});
    virtual ~App();

    bool init();
    void run();
    void stop();

    [[nodiscard]] bool is_running() const { return is_running_; }
    [[nodiscard]] Window& window() { return window_; }
    [[nodiscard]] InputManager& input_manager() { return input_manager_; }
    [[nodiscard]] const AppConfig& config() const { return config_; }
    [[nodiscard]] const JudgmentConstants& judgment_constants() const { return judgment_constants_; }
    // #89: Cabinet theme textures + bitmap digits, loaded once in init().
    [[nodiscard]] const ThemeTextures& theme_textures() const { return theme_textures_; }
    // #90: TrueType text (theme fonts + per-size atlases), loaded in init() and
    // re-baked from run() when the window height changes. Non-const: lazy bakes.
    [[nodiscard]] TextRenderer& text_renderer() { return text_renderer_; }
    // #92: the Cel noteskin for the title/attract art, loaded once in init()
    // (skipped headless). Gameplay still owns its own instance.
    [[nodiscard]] const NoteSkin& noteskin() const { return noteskin_; }
    [[nodiscard]] uint64_t input_reference_ns() const { return input_reference_ns_; }
    [[nodiscard]] const FrameStats& frame_stats() const { return frame_stats_; }

    using UpdateCallback = std::function<void(double fixed_dt)>;
    using RenderCallback = std::function<void(double alpha)>;
    // Returns true if the callback consumed the event (e.g. a screen handled Back).
    using EventCallback = std::function<bool(const SDL_Event&)>;

    void set_update_callback(UpdateCallback cb) { update_cb_ = std::move(cb); }
    void set_render_callback(RenderCallback cb) { render_cb_ = std::move(cb); }
    void set_event_callback(EventCallback cb) { event_cb_ = std::move(cb); }

protected:
    virtual void on_update(double fixed_dt);
    virtual void on_render(double alpha);
    virtual void on_event(const SDL_Event& event);

private:
    void process_events();
    void print_perf_report() const;

    AppConfig config_;
    Window window_;
    InputManager input_manager_;
    JudgmentConstants judgment_constants_;
    ThemeTextures theme_textures_;
    TextRenderer text_renderer_;
    NoteSkin noteskin_;
    FrameStats frame_stats_;
    bool is_running_ = false;
    uint64_t perf_frequency_ = 0;
    uint64_t last_time_ = 0;
    uint64_t input_reference_ns_ = 0;
    double accumulator_ = 0.0;
    int frames_rendered_ = 0;

    UpdateCallback update_cb_;
    RenderCallback render_cb_;
    EventCallback event_cb_;
};

} // namespace blaze4k
