#include "gameplay/gameplay_view.hpp"

#include <array>
#include <iomanip>
#include <iostream>

#include "gameplay/judgment_input.hpp"

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

bool GameplayView::init(const Chart& chart, const JudgmentConstants& constants,
                        const std::string& audio_path, const GameplayOptions& options) {
    chart_ = chart;
    if (chart_.notes.empty()) {
        std::cerr << "[GameplayView] Chart has no notes; nothing to play\n";
        return false;
    }

    judge_.reset(&chart_, &constants);
    score_.reset(&chart_, &constants);
    life_.set_fail_enabled(options.fail_enabled);
    life_.reset(&chart_, &constants);
    exited_ = false;

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
              << ", time source " << (use_stub_ ? "stub" : "audio")
              << ", fail " << (options.fail_enabled ? "enabled" : "off") << "\n";
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

void GameplayView::handle_input_events(const std::vector<InputEvent>& events, uint64_t reference_ns) {
    // Once failed, gameplay input is closed: forwarding steps would append
    // judgment events that `update` never drains after the fail transition.
    if (!ready_ || exited_) {
        return;
    }

    // Sample the music time once, age each SDL-timestamped event against it, and
    // hand the engine an absolute music time (no frame/wall-clock enters here).
    const double reference_music = clock_.time_seconds();
    for (const InputEvent& event : events) {
        if (!event.pressed) {
            continue;
        }

        int column = -1;
        switch (event.action) {
            case GameAction::Left: column = 0; break;
            case GameAction::Down: column = 1; break;
            case GameAction::Up: column = 2; break;
            case GameAction::Right: column = 3; break;
            default: break;
        }
        if (column < 0) {
            continue; // Menu actions and releases are not gameplay steps.
        }

        judge_.handle_step(column,
                           music_time_for_event(event.timestamp_ns, reference_ns, reference_music));
    }
}

void GameplayView::update(double fixed_dt, const std::array<bool, 4>& held_columns) {
    if (!ready_) {
        return;
    }

    if (use_stub_) {
        // Stub fallback: retry starting the stream in case it was only transiently
        // unavailable at init. If it starts, switch the clock to the real audio
        // source so gameplay time always follows whichever source drives update.
        if (!audio_started_ && audio_.is_loaded() && audio_.play()) {
            audio_started_ = true;
            use_stub_ = false;
            bind_clock_source();
            std::cout << "[GameplayView] Audio started; switched clock source from stub to audio\n";
        } else {
            stub_frames_ += fixed_dt * static_cast<double>(stub_sample_rate_);
        }
    }

    // Once failed, gameplay has ended: keep the stub clock advancing (demo only)
    // but stop judging and draining events.
    if (exited_) {
        return;
    }

    // Judgments derive from the music clock only; `fixed_dt` advances the demo
    // stub source and never reaches the engine.
    judge_.update(clock_.time_seconds(), held_columns);

    // Scoring/life are event-sourced: drain the newly appended judgment events into
    // both keepers. No independent judgment logic, no frame/wall-clock input.
    new_events_.clear();
    judge_.drain_new_events(new_events_);
    score_.consume(new_events_);
    life_.consume(new_events_);

    if (life_.has_failed()) {
        exited_ = true;
        // Capture the fail time before pausing: `stop()` would seek to frame 0,
        // making the log read time 0 and snapping `render()` back to song start.
        // `pause()` halts playback while preserving the stream position.
        const double fail_time = clock_.time_seconds();
        audio_.pause();
        std::cerr << "[GameplayView] Failed: life empty at " << fail_time << "s\n";
    }
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

    // "Visual feedback matches the log": filter the rendered set to exactly the
    // notes the judgment log has not hidden (OpenITG hide rule, Player.cpp:1284-1302).
    visible_items_.clear();
    for (const NoteRenderItem& item : items_) {
        const int index =
            item.note == nullptr ? -1 : static_cast<int>(item.note - chart_.notes.data());
        if (!judge_.is_note_hidden(index)) {
            visible_items_.push_back(item);
        }
    }

    field_renderer_.render(field_, visible_items_, screen_w, screen_h, skin_, renderer);

    // Live HUD. Only reached with a valid GL context (`render()` above early-returns
    // when the renderer is uninitialized); the score/life state is computed in update().
    hud_.render(score_.state(), screen_w, screen_h, renderer);
    hud_.render_life(life_.life(), screen_w, screen_h, renderer);
}

GameplayOutcome GameplayView::outcome() const {
    if (life_.has_failed()) {
        return GameplayOutcome::Failed;
    }
    if (score_.is_complete()) {
        return GameplayOutcome::Cleared;
    }
    return GameplayOutcome::InProgress;
}

void GameplayView::shutdown() {
    if (ready_) {
        const ScoreState& score = score_.state();
        std::cout << "[GameplayView] Score: DP " << score.actual_dp << "/" << score.possible_dp
                  << " (" << std::fixed << std::setprecision(2) << (score.percent * 100.0)
                  << "%) grade " << score_.grade().label << " | combo " << score.combo
                  << " (max " << score.max_combo << ")";
        std::cout << " | F " << score.tap_counts[static_cast<std::size_t>(TapJudgment::Fantastic)]
                  << " E " << score.tap_counts[static_cast<std::size_t>(TapJudgment::Excellent)]
                  << " G " << score.tap_counts[static_cast<std::size_t>(TapJudgment::Great)]
                  << " D " << score.tap_counts[static_cast<std::size_t>(TapJudgment::Decent)]
                  << " W " << score.tap_counts[static_cast<std::size_t>(TapJudgment::WayOff)]
                  << " M " << score.tap_counts[static_cast<std::size_t>(TapJudgment::Miss)]
                  << " Mine " << score.tap_counts[static_cast<std::size_t>(TapJudgment::HitMine)]
                  << " OK " << score.hold_counts[static_cast<std::size_t>(HoldJudgment::Ok)]
                  << " NG " << score.hold_counts[static_cast<std::size_t>(HoldJudgment::Ng)]
                  << " | events " << judge_.events().size()
                  << " | life " << std::fixed << std::setprecision(3) << life_.life()
                  << (life_.has_failed() ? " FAILED" : " alive") << "\n";
        audio_.stop();
        audio_.unload();
        skin_.shutdown();
        clock_.clear_source();
        items_.clear();
        visible_items_.clear();
        ready_ = false;
    }
}

} // namespace td
