#include "screens/calibration_screen.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <utility>

#include "gameplay/judgment_input.hpp"
#include "data/config.hpp"
#include "render/gl_quad_renderer.hpp"
#include "render/theme_layout.hpp"
#include "render/theme_textures.hpp"
#include "screens/options_menu.hpp"
#include "screens/screen_manager.hpp"
#include "screens/select_art.hpp"
#include "screens/setup_art.hpp"

namespace blaze4k {

namespace {

constexpr double kFallbackBpm = 120.0;

[[nodiscard]] bool is_panel_action(GameAction action) {
    switch (action) {
        case GameAction::Left:
        case GameAction::Down:
        case GameAction::Up:
        case GameAction::Right:
            return true;
        default:
            return false;
    }
}

} // namespace

CalibrationScreen::CalibrationScreen() = default;

CalibrationScreen::CalibrationScreen(std::filesystem::path click_wav_path)
    : click_path_(std::move(click_wav_path)) {}

CalibrationScreen::CalibrationScreen(IAudioStream& stream, MusicClock::Source source,
                                     CalibrationConfig config)
    : calib_(config), config_(config), injected_stream_(&stream) {
    if (source) {
        source_injected_ = true;
        injected_source_ = std::move(source);
    } else {
        metronome_ = Metronome(stream);
    }
}

void CalibrationScreen::enter(ScreenContext& /*ctx*/) {
    calib_ = OffsetCalibration(config_);
    result_ = CalibrationResult{};
    saved_ = false;
    synthetic_ = false;
    stub_frames_ = 0.0;
    phase_ = CalibrationPhase::CountIn;
    clock_.set_global_offset_seconds(0.0);

    if (source_injected_) {
        clock_.set_source(injected_source_);
        return;
    }

    MetronomeConfig metronome_config;
    metronome_config.bpm = config_.beat_period_seconds > 0.0
                               ? 60.0 / config_.beat_period_seconds
                               : kFallbackBpm;
    metronome_config.lead_in_seconds = config_.lead_in_seconds;
    metronome_config.beats = std::max(1, config_.max_beats);

    if (metronome_.prepare(click_path_, metronome_config)) {
        metronome_.start();
    }

    if (metronome_.is_playing()) {
        clock_.set_source(metronome_.clock_source());
    } else {
        synthetic_ = true;
        std::cerr << "[Calibration] audio unavailable; using synthetic clock\n";
        clock_.set_source([this] {
            return SamplePosition{static_cast<uint64_t>(stub_frames_), stub_rate_};
        });
    }
}

void CalibrationScreen::update(ScreenContext& ctx, double fixed_dt,
                               const std::vector<InputEvent>& events) {
    if (synthetic_) {
        stub_frames_ += fixed_dt * static_cast<double>(stub_rate_);
    }

    const double reference_music = clock_.time_seconds();
    if (phase_ == CalibrationPhase::CountIn && reference_music >= config_.lead_in_seconds) {
        phase_ = CalibrationPhase::Sampling;
    }

    for (const InputEvent& event : events) {
        if (!event.pressed) {
            continue;
        }

        if (is_panel_action(event.action) && phase_ != CalibrationPhase::CountIn) {
            const double hit =
                music_time_for_event(event.timestamp_ns, ctx.input_reference_ns, reference_music);
            const int index = config_.nearest_beat_index(hit);
            calib_.add_sample(config_.beat_time(index), hit);
        }

        if (event.action == GameAction::Confirm && calib_.ready()) {
            if (synthetic_) {
                std::cerr << "[Calibration] audio unavailable; offset not saved\n";
            } else if (ctx.config == nullptr) {
                std::cerr << "[Calibration] no config available; offset not saved\n";
            } else {
                ctx.config->offset.global_offset_seconds = calib_.result().offset_seconds;
                saved_ = true;
                if (ctx.manager != nullptr) {
                    ctx.manager->transition_to(ScreenId::Select);
                }
            }
        }
    }

    result_ = calib_.result();
    if (calib_.ready()) {
        phase_ = CalibrationPhase::Ready;
    }
}

void CalibrationScreen::render(ScreenContext& ctx, GlQuadRenderer& renderer, int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }

    const theme::LayoutScale L = theme::layout_scale(w, h);
    const ThemeTextures* theme = ctx.theme;
    TextRenderer* text = ctx.text;
    setup_art::draw_chrome(theme, text, renderer, L, w, h, setup_art::kCalibrateTitle);

    const char* phase_text = "GET READY";
    switch (phase_) {
        case CalibrationPhase::CountIn:
            phase_text = "GET READY";
            break;
        case CalibrationPhase::Sampling:
            phase_text = "TAP ON THE BEAT";
            break;
        case CalibrationPhase::Ready:
            phase_text = "DONE";
            break;
    }

    // Synthetic mode measures against a fixed_dt stub clock, so the number is
    // provisional and unsavable; the notice says so and the OFFSET plate keeps
    // "---" rather than imply a usable result. The samples text and "---" are
    // SSO-sized; the formatted offset (once ready) is not allocation-free.
    const bool offset_ready = result_.ready && !synthetic_;
    const std::string samples =
        setup_art::calibration_samples_text(sample_count(), config_.min_samples, result_.ready);
    const std::string offset = offset_ready ? format_offset(result_.offset_seconds)
                                            : std::string(setup_art::kOffsetPending);
    setup_art::draw_calibration(theme, text, renderer, L,
                                setup_art::CalibrationView{phase_text, samples, offset,
                                                           offset_ready, synthetic_});

    // ENTER SAVE only once Confirm can save (a real clock and a ready result).
    if (!offset_ready) {
        setup_art::draw_hint_bar(theme, text, renderer, L, w, setup_art::kCalibrateNoAudioHint);
    } else {
        setup_art::draw_hint_bar(theme, text, renderer, L, w, setup_art::kCalibrateHint);
    }

    if (theme != nullptr) {
        select_art::draw_scanlines(*theme, renderer, w, h, L);
    }
}

void CalibrationScreen::exit(ScreenContext& /*ctx*/) {
    metronome_.stop();
    clock_.clear_source();
}

} // namespace blaze4k
