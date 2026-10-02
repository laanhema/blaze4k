#pragma once

#include <array>
#include <cstddef>
#include <vector>
#include "chart/chart.hpp"
#include "gameplay/judgment.hpp"
#include "timing/judgment_constants.hpp"

namespace blaze4k {

// Deterministic, event-sourced judgment engine.
//
// This module is time-parameterized: callers pass an absolute music time (from
// `MusicClock`) rather than letting the engine read a clock, exactly like
// `NoteField`. It includes no SDL/GL/audio/chrono headers, so the judgment path
// can never consult wall-clock or frame timing (AGENTS.md core principle 1).
//
// It owns the append-only judgment log and nothing else: no combo, life, DP,
// percent, or grade state. Those derive from the log (B5/B6).
//
// The one side channel is a display-only queue for OpenITG MercifulBeginner's
// early Way Off (Player.cpp:1089-1093): on a Beginner chart an early step that
// grades as Way Off is only shown, never recorded. Those events never enter the
// log, so score, life, combo and results cannot see them.
class JudgmentEngine {
public:
    // `chart` and `constants` must outlive the engine. Builds per-column note
    // index lists and resets all note/judgment state.
    void reset(const Chart* chart, const JudgmentConstants* constants);

    // Button-down in `column` at `music_time_seconds`. Mirrors Player::Step /
    // HandleStep: closest-note tap/mine grading + roll re-hit refresh. Also
    // records the press instant (for pad-stick, OpenITG `GetSecsHeld`).
    void handle_step(int column, double music_time_seconds);

    // Per-frame: miss/avoided-mine expiry, hold/roll life, and held-over-mine
    // crossing (Player::CrossedMineRow) for mines crossed since the previous
    // update, evaluated `pad_stick` behind the music. Mirrors Player::Update.
    // Uses only `music_time_seconds` (never frame delta).
    void update(double music_time_seconds, const std::array<bool, 4>& held_columns);

    [[nodiscard]] const std::vector<JudgmentEvent>& events() const { return events_; }
    void drain_new_events(std::vector<JudgmentEvent>& out); // appends events since last drain

    // Appends, then clears, the display-only events (MercifulBeginner early Way
    // Off; kind Tap, window WayOff, negative delta_ms). Presentation only: they
    // are never in events() / latest_event() / drain_new_events(), and must
    // never reach score, life, combo or results. Cleared by reset().
    void drain_display_only_events(std::vector<JudgmentEvent>& out);

    // True when the chart passed to reset() is a Beginner chart (case-insensitive
    // "beginner" label). Whether the merciful rules apply also depends on
    // JudgmentConstants::merciful_beginner.
    [[nodiscard]] bool is_beginner() const { return is_beginner_; }

    [[nodiscard]] bool is_note_judged(int note_index) const;
    [[nodiscard]] bool is_note_hidden(int note_index) const;

    // True while a hold/roll head has been hit but its outcome (OK/NG) is still
    // pending. The note head is already judged (and possibly hidden), but the
    // body must keep rendering as the player holds it down.
    [[nodiscard]] bool is_hold_in_progress(int note_index) const;
    // True once a hold/roll head has been graded as hit (even after OK/NG).
    [[nodiscard]] bool is_hold_head_hit(int note_index) const;
    // Hold/roll outcome; Num while pending or for non-hold notes.
    [[nodiscard]] HoldJudgment hold_judgment(int note_index) const;
    // Music time the hold was last held (a roll: last re-hit); where a let-go
    // hold's body is drawn from. Only meaningful once the head was hit.
    [[nodiscard]] double hold_last_held_seconds(int note_index) const;
    [[nodiscard]] const JudgmentEvent* latest_event() const;

private:
    struct NoteState {
        TapJudgment tap = TapJudgment::Num;     // Num = ungraded
        HoldJudgment hold = HoldJudgment::Num;
        double hold_satisfied_time = 0.0;       // last music time life was full
        bool hold_head_hit = false;             // bSteppedOnTapNote
        bool complete = false;                  // no further judgment possible
    };

    void emit(const JudgmentEvent& event);
    // Shared step routine (OpenITG Player::Step): used by both deliberate
    // presses and held-over-mine crossings.
    void step(int column, double music_time_seconds);
    void handle_step_tap(int note_index, double delta_seconds, double hit_time);
    void handle_step_mine(int note_index, double delta_seconds, double hit_time);
    void refresh_active_rolls(int column, double music_time);
    void expire_notes(double music_time);
    void update_holds(double music_time, const std::array<bool, 4>& held_columns);
    void cross_mines(double music_time, const std::array<bool, 4>& held_columns);

    const Chart* chart_ = nullptr;
    const JudgmentConstants* constants_ = nullptr;
    std::vector<NoteState> states_;
    std::vector<std::vector<int>> column_notes_;   // note indices per column (time order)
    std::vector<std::vector<int>> column_mines_;   // mine indices per column (time order)
    std::vector<int> active_holds_;                // hold/roll indices not yet complete
    std::vector<JudgmentEvent> events_;
    std::size_t new_event_begin_ = 0;
    // MercifulBeginner: chart is Beginner (from reset()); display-only queue.
    bool is_beginner_ = false;
    std::vector<JudgmentEvent> display_only_events_;
    bool has_last_update_ = false;
    double last_update_time_ = 0.0;
    // Forward-only mine-crossing cursor (music time - pad_stick); mines at or
    // before it have crossed. Not rewound by a backward clock resync.
    double mine_cursor_ = 0.0;
    // Music time of the latest button-down per column (-inf = none recorded).
    std::array<double, 4> last_press_time_{};
};

} // namespace blaze4k
