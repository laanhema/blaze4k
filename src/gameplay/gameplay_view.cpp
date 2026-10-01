#include "gameplay/gameplay_view.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <iostream>

#include "gameplay/judgment_input.hpp"

namespace blaze4k {

namespace {

// Ticks are handed to the audio engine this far ahead of their note so each one
// gets a sample-accurate scheduled start despite frame timing (StepMania's
// PlayTicks looks ahead 0.25 s the same way).
constexpr double kAssistTickLookaheadSeconds = 0.25;
// A tick first seen later than this past its note (e.g. after a long hitch) is
// dropped rather than sounding audibly off the beat.
constexpr double kAssistTickLateSeconds = 0.05;

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
                        const std::string& audio_path, const GameplayOptions& options,
                        const std::string& background_path) {
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
    final_combo_celebrated_ = false;
    judge_anim_.reset();
    was_held_.fill(false);
    press_time_.fill(-1e9);
    explosions_.fill(ColumnExplosion{});
    mine_explosion_start_.fill(-1e9);

    field_.set_chart(&chart_);
    field_.set_speed_mod(options.speed);

    config_ = NoteFieldConfig{};
    config_.pixels_per_beat = 64.0;
    config_.column_width = NoteSkin::kColumnWidth;
    config_.note_height = NoteSkin::kNoteSize;
    config_.direction = options.scroll;
    config_.receptor_y = 0.0; // resolved per-frame from the framebuffer height
    field_.set_config(config_);

    receptor_fraction_ = (options.scroll == ScrollDirection::Down) ? 0.85 : 0.15;
    clock_.set_global_offset_seconds(options.global_offset_seconds);

    assist_tick_ = false;
    if (options.assist_tick) {
        assist_schedule_.reset(chart_);
        assist_tick_ = assist_player_.init(assist_tick_path_);
    }

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
    background_.load(background_path);

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
              << ", fail " << (options.fail_enabled ? "enabled" : "off")
              << ", assist tick " << (assist_tick_ ? "on" : "off") << "\n";
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
    // but stop judging and draining events. Popups keep fading out (presentation
    // only) instead of freezing on their last frame.
    if (exited_) {
        judge_anim_.update(fixed_dt, score_.state().combo);
        return;
    }

    // Judgments derive from the music clock only; `fixed_dt` advances the demo
    // stub source and never reaches the engine.
    judge_.update(clock_.time_seconds(), held_columns);

    for (std::size_t column = 0; column < held_columns.size(); ++column) {
        if (held_columns[column] && !was_held_[column]) {
            press_time_[column] = clock_.time_seconds();
        }
    }
    was_held_ = held_columns;

    if (assist_tick_ && !use_stub_) {
        schedule_assist_ticks();
    }

    // Scoring/life are event-sourced: drain the newly appended judgment events into
    // both keepers. No independent judgment logic, no frame/wall-clock input.
    new_events_.clear();
    judge_.drain_new_events(new_events_);
    score_.consume(new_events_);
    life_.consume(new_events_);
    judge_anim_.consume(new_events_);
    judge_anim_.update(fixed_dt, score_.state().combo);
    for (const JudgmentEvent& event : new_events_) {
        arm_explosion(event);
    }

    // OQ2: the final combo also pops once the chart is fully resolved (the
    // milestone path covers every 50; this covers the last, non-round count).
    if (!final_combo_celebrated_ && score_.is_complete()) {
        final_combo_celebrated_ = true;
        judge_anim_.celebrate(score_.state().combo);
    }

    if (life_.has_failed()) {
        exited_ = true;
        // Capture the fail time before pausing: `stop()` would seek to frame 0,
        // making the log read time 0 and snapping `render()` back to song start.
        // `pause()` halts playback while preserving the stream position.
        const double fail_time = clock_.time_seconds();
        audio_.pause();
        assist_player_.stop_all();
        std::cerr << "[GameplayView] Failed: life empty at " << fail_time << "s\n";
    }
}

void GameplayView::render(GlQuadRenderer& renderer, int screen_w, int screen_h) {
    if (!ready_ || !renderer.is_initialized() || screen_h <= 0 || screen_w <= 0) {
        return;
    }

    // Background first (behind receptors/notes/HUD), dimmed for readability.
    background_.render(renderer, screen_w, screen_h);

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

    const double now = clock_.time_seconds();
    NoteFieldFrame frame;

    // "Visual feedback matches the log": filter the rendered set to exactly the
    // notes the judgment log has not hidden (OpenITG hide rule, Player.cpp:1284-1302).
    // A hold/roll whose head was hit is drawn from its judgment state instead:
    //  - active (hold: button down; roll: still alive): head pinned in the
    //    receptor, active art, hold explosion;
    //  - let go (released while pending, or NG): inactive art, the body resuming
    //    from where it was last held and scrolling past (StepMania draws a let-go
    //    hold from its last held beat);
    //  - OK: finished, nothing left to draw.
    visible_items_.clear();
    for (const NoteRenderItem& item : items_) {
        const int index =
            item.note == nullptr ? -1 : static_cast<int>(item.note - chart_.notes.data());
        if (item.has_body && judge_.is_hold_head_hit(index)) {
            if (judge_.hold_judgment(index) == HoldJudgment::Ok) {
                continue;
            }
            const auto column = static_cast<std::size_t>(item.column);
            const bool roll = item.type == NoteType::RollHead;
            const bool held = column < was_held_.size() && was_held_[column];
            NoteRenderItem hold = item;
            if (judge_.is_hold_in_progress(index) && (roll || held)) {
                hold.held = true;
                if (!exited_ && column < frame.hold_explosion.size()) {
                    frame.hold_explosion[column] = true;
                }
            } else {
                const double last_held_beat =
                    chart_.timing.seconds_to_beat(judge_.hold_last_held_seconds(index));
                hold.head_offset = std::clamp(field_.offset_for_beat(last_held_beat, now),
                                              item.head_offset, item.tail_offset);
            }
            visible_items_.push_back(hold);
            continue;
        }
        if (judge_.is_note_hidden(index)) {
            continue;
        }
        visible_items_.push_back(item);
    }

    // Skin animation runs on song beats of the music clock, like StepMania's
    // beat-based noteskin animation; the receptor press bump is the Cel
    // ReceptorArrow NoneCommand (zoom 0.75 -> 1 over 0.11 s).
    frame.music_seconds = now;
    frame.beat = chart_.timing.seconds_to_beat(frame.music_seconds);
    for (std::size_t column = 0; column < frame.receptor_zoom.size(); ++column) {
        const double since_press = frame.music_seconds - press_time_[column];
        frame.receptor_zoom[column] =
            static_cast<float>(0.75 + 0.25 * std::clamp(since_press / 0.11, 0.0, 1.0));
        // After a fail the clock is paused; a frozen half-faded flash would stick.
        if (!exited_) {
            const ColumnExplosion& explosion = explosions_[column];
            frame.tap_explosion[column] = TapExplosionState{
                explosion.window, now - explosion.start_seconds, explosion.duration};
            frame.mine_explosion_elapsed[column] = now - mine_explosion_start_[column];
        }
    }
    field_renderer_.render(field_, visible_items_, frame, screen_w, screen_h, skin_, renderer);

    // Live HUD. Only reached with a valid GL context (`render()` above early-returns
    // when the renderer is uninitialized); the score/life state is computed in update().
    hud_.render(score_.state(), screen_w, screen_h, renderer);
    hud_.render_life(life_.life(), screen_w, screen_h, renderer);
    judge_anim_.render(renderer, screen_w, screen_h);
}

void GameplayView::schedule_assist_ticks() {
    // A tick at note time T sounds when the *stream* reaches T, i.e. on the beat
    // heard in the music, WITHOUT the global offset (StepMania's PlayTicks uses the
    // NoOffset timing calls for the same reason). The offset is calibrated as
    // -(tap lag behind a click at stream time b), so a player tapping on a tick at
    // stream time T lands on clock time T: a Fantastic. Scheduling on the offset
    // clock instead delays the tick by -offset (a -0.2 s offset = 200 ms late).
    // The stream cursor advances in lockstep with the engine clock the tick is
    // scheduled on, so remaining stream time maps 1:1 onto engine time (no rate
    // mods in scope).
    const double now = clock_.sample_time_seconds();
    due_ticks_.clear();
    assist_schedule_.collect_due(now + kAssistTickLookaheadSeconds, due_ticks_);
    for (const double tick_time : due_ticks_) {
        const double until = tick_time - now;
        if (until >= -kAssistTickLateSeconds) {
            assist_player_.play_in(until);
        }
    }
}

void GameplayView::arm_explosion(const JudgmentEvent& event) {
    if (event.column < 0 || event.column >= static_cast<int>(explosions_.size())) {
        return;
    }
    const auto column = static_cast<std::size_t>(event.column);
    if (event.kind == JudgmentKind::HitMine) {
        // Stepped on or held over; its own actor, so it layers over a tap flash.
        mine_explosion_start_[column] = event.hit_time_seconds;
        return;
    }
    ColumnExplosion& explosion = explosions_[column];
    if (event.kind == JudgmentKind::Tap) {
        // Fantastic..WayOff map onto the W1..W5 explosions.
        explosion = ColumnExplosion{event.window, event.hit_time_seconds, kCelTapExplosionSeconds};
    } else if (event.kind == JudgmentKind::HoldOk || event.kind == JudgmentKind::RollOk) {
        // Cel HeldCommand: a short W1 flash as the hold ends.
        explosion = ColumnExplosion{TapJudgment::Fantastic, event.hit_time_seconds,
                                    kCelHeldExplosionSeconds};
    }
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
        assist_player_.shutdown();
        assist_tick_ = false;
        skin_.shutdown();
        background_.shutdown();
        clock_.clear_source();
        items_.clear();
        visible_items_.clear();
        judge_anim_.reset();
        ready_ = false;
    }
}

} // namespace blaze4k
