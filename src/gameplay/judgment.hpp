#pragma once

#include "chart/note.hpp"
#include "timing/judgment_constants.hpp"

namespace blaze4k {

// Immutable, event-sourced judgment model (PRD section 6 pattern 2).
//
// Every hit evaluated by the engine is appended to an append-only session log as
// a `JudgmentEvent`; scoring, combo, life, results, and future replays all derive
// from that log rather than re-judging. This header is a pure value-type module:
// no clocks, no platform headers.
enum class JudgmentKind {
    Tap,          // tap / hold-head / roll-head graded against a tap window
    Miss,         // untouched tap / hold-head / roll-head expired
    HitMine,      // mine triggered (stepped on, or held over)
    AvoidedMine,  // mine passed untouched (no penalty; stats only)
    HoldOk,
    HoldNg,
    RollOk,
    RollNg,
    RollHit,      // roll re-hit refresh (visual only; no score/combo)
};

struct JudgmentEvent {
    JudgmentKind kind = JudgmentKind::Tap;
    int column = 0;
    double note_time_seconds = 0.0;
    double hit_time_seconds = 0.0;          // music time of input / expiry
    double delta_ms = 0.0;                  // (hit - note)*1000; negative = early
    TapJudgment window = TapJudgment::Num;  // tap classification; Num for hold outcomes
    HoldJudgment hold = HoldJudgment::Num;  // hold/roll outcome; Num otherwise
    NoteType note_type = NoteType::Tap;
    int note_index = -1;                    // index into Chart::notes
};

// OpenITG hides a judged note only when score >= TNS_GREAT (Player.cpp:1284-1302).
[[nodiscard]] constexpr bool is_tap_window_hidden(TapJudgment j) {
    return j == TapJudgment::Fantastic || j == TapJudgment::Excellent || j == TapJudgment::Great;
}

} // namespace blaze4k
