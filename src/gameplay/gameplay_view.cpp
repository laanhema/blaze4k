#include "gameplay/gameplay_view.hpp"

#include <iostream>

namespace td {

namespace {

const char* speed_type_name(SpeedModType type) {
    switch (type) {
        case SpeedModType::CMod: return "C-mod";
        case SpeedModType::XMod: return "X-mod";
        case SpeedModType::MMod: return "M-mod";
    }
    return "X-mod";
}

} // namespace

GameplayView::~GameplayView() {
    shutdown();
}

bool GameplayView::init(const Chart& chart, const std::string& audio_path, const GameplayOptions& options) {
    chart_ = chart;
    if (chart_.notes.empty()) {
        std::cerr << "[GameplayView] Chart has no notes; nothing to play\n";
        return false;
    }

    field_.set_chart(&chart_);
    field_.set_speed_mod(options.speed);

    config_ = NoteFieldConfig{};
    config_.pixels_per_beat = 64.0;
    config_.column_width = 64.0;
    config_.direction = options.scroll;
    config_.receptor_y = 0.0; // resolved per-frame from the framebuffer height
    field_.set_config(config_);

    receptor_fraction_ = (options.scroll == ScrollDirection::Down) ? 0.85 : 0.15;
    clock_.set_global_offset_seconds(options.global_offset_seconds);

    if (!audio_path.empty() && audio_.load(audio_path)) {
        audio_started_ = audio_.play();
    }

    if (!audio_started_) {
        use_stub_ = true;
        std::cerr << "[GameplayView] Audio unavailable; using synthetic stub clock "
                     "(demo harness only)\n";
    }

    bind_clock_source();
    skin_.init();

    ready_ = true;
    std::cout << "[GameplayView] Loaded chart '" << chart_.difficulty << "' (meter " << chart_.meter
              << "): taps=" << chart_.tap_count << " holds=" << chart_.hold_count
              << " rolls=" << chart_.roll_count << " mines=" << chart_.mine_count
              << " total=" << chart_.notes.size() << "\n";
    std::cout << "[GameplayView] Speed " << speed_type_name(options.speed.type) << " "
              << options.speed.value;
    if (options.speed.type == SpeedModType::CMod) {
        std::cout << " (time-spacing " << (options.speed.value / 60.0) << " beats/s)";
    } else {
        std::cout << " (x-speed " << field_.effective_x_speed() << ")";
    }
    std::cout << ", " << (options.scroll == ScrollDirection::Down ? "downscroll" : "upscroll")
              << ", time source " << (use_stub_ ? "stub" : "audio") << "\n";
    return true;
}

void GameplayView::bind_clock_source() {
    if (use_stub_) {
        clock_.set_source([this] {
            return SamplePosition{
                static_cast<uint64_t>(stub_frames_),
                static_cast<uint32_t>(stub_sample_rate_),
            };
        });
    } else {
        clock_.set_source([this] {
            return SamplePosition{audio_.get_position_frames(), audio_.get_sample_rate()};
        });
    }
}

void GameplayView::update(double fixed_dt) {
    if (!ready_) {
        return;
    }

    if (!use_stub_) {
        return;
    }

    // Stub fallback: retry starting the stream in case it was only transiently
    // unavailable at init. If it starts, switch the clock to the real audio
    // source so gameplay time always follows whichever source drives update.
    if (!audio_started_ && audio_.is_loaded() && audio_.play()) {
        audio_started_ = true;
        use_stub_ = false;
        bind_clock_source();
        std::cout << "[GameplayView] Audio started; switched clock source from stub to audio\n";
        return;
    }

    stub_frames_ += fixed_dt * static_cast<double>(stub_sample_rate_);
}

void GameplayView::render(GlQuadRenderer& renderer, int screen_w, int screen_h) {
    if (!ready_ || !renderer.is_initialized() || screen_h <= 0 || screen_w <= 0) {
        return;
    }

    const double receptor_y = receptor_fraction_ * static_cast<double>(screen_h);
    config_.receptor_y = receptor_y;
    field_.set_config(config_);

    double visible_top = 0.0;
    double visible_bottom = 0.0;
    if (config_.direction == ScrollDirection::Down) {
        visible_top = receptor_y - static_cast<double>(screen_h);
        visible_bottom = receptor_y;
    } else {
        visible_top = -receptor_y;
        visible_bottom = static_cast<double>(screen_h) - receptor_y;
    }

    field_.compute_visible(clock_.time_seconds(), visible_top, visible_bottom, items_);
    field_renderer_.render(field_, items_, screen_w, screen_h, skin_, renderer);
}

void GameplayView::shutdown() {
    if (ready_) {
        audio_.stop();
        audio_.unload();
        skin_.shutdown();
        clock_.clear_source();
        items_.clear();
        ready_ = false;
    }
}

} // namespace td
