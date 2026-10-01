#pragma once

#include <cstddef>
#include <vector>

#include "chart/chart.hpp"
#include "gameplay/judgment.hpp"
#include "timing/judgment_constants.hpp"

namespace blaze4k {

// Live life derived solely from the B4 judgment log (PRD section 6 pattern 2).
//
// Pure value type: no clocks, no platform headers. Every field is recomputed from
// the append-only `JudgmentEvent` stream, exactly like OpenITG's `LifeMeterBar`
// derives its percentage from judged rows (never from frame timing).
struct LifeState {
    // OpenITG `DRAIN_NORMAL` starts at the `LifeMeterBar` `InitialValue` theme
    // metric; the fallback theme sets 0.5 (LifeMeterBar.cpp:23-27;
    // fallback/metrics.ini:2385). This deliberately overrides the issue AC's
    // "starts full" wording in favor of OpenITG parity.
    double life = 0.5;   // clamped [0,1]
    bool failed = false; // fail-enabled and life reached 0
};

// Pure, event-sourced life meter.
//
// Consumes the B4 `JudgmentEvent` log and aggregates per-note tap events into
// OpenITG rows (row identity = exact `Note.beat`, the same rule B5 `ScoreKeeper`
// uses), applying exactly one life delta per completed row. Hold/roll outcomes and
// hit mines apply individually. It includes no platform, audio, or time headers, so
// the life path can never consult wall-clock or frame timing (AGENTS.md principle 1).
//
// It owns life/fail only: no score, combo, DP, percent, or grade (B5).
//
// NOTE: the chart-derived row table below duplicates `ScoreKeeper::reset`'s
// grouping (B5 is frozen and exposes no resolved-row stream). A future refactor
// could extract a shared helper; for now the two must be kept in sync.
class LifeKeeper {
public:
    // `chart` and `constants` must outlive the keeper. Builds the chart-derived
    // row tables and resets life to the OpenITG `InitialValue` (0.5).
    // Precondition: `chart->notes` is beat-sorted with equal beats contiguous
    // (the NoteParser guarantee).
    void reset(const Chart* chart, const JudgmentConstants* constants);

    // FAIL_IMMEDIATE (true, default) / FAIL_OFF (false). Set before gameplay.
    void set_fail_enabled(bool enabled) { fail_enabled_ = enabled; }

    // Consume the append-only B4 log; idempotent per note.
    void consume(const JudgmentEvent& event);
    void consume(const std::vector<JudgmentEvent>& events);

    [[nodiscard]] const LifeState& state() const { return state_; }
    [[nodiscard]] double life() const { return state_.life; }
    [[nodiscard]] bool is_failing() const { return state_.life <= 0.0; }
    [[nodiscard]] bool has_failed() const { return state_.failed; }
    [[nodiscard]] bool fail_enabled() const { return fail_enabled_; }

private:
    struct RowAggregate {
        int expected = 0;            // tap/hold-head notes in this row
        int judged = 0;
        bool has_miss = false;
        double last_delta_ms = 0.0;  // greatest offset wins; ties -> later column
        int last_column = -1;
        TapJudgment last_window = TapJudgment::Num;
    };

    void consume_tap_like(const JudgmentEvent& event, bool miss);
    void consume_hold_outcome(const JudgmentEvent& event);
    void consume_event(const JudgmentEvent& event);
    void resolve_row(int row);
    [[nodiscard]] bool is_hot() const { return state_.life >= 1.0; }
    [[nodiscard]] double delta_for_tap(TapJudgment window) const;
    [[nodiscard]] double delta_for(const JudgmentEvent& event) const;
    void apply(double delta);
    // OpenITG sets `bFailed` once per frame (ScreenGameplay.cpp:1471-1493), not per
    // ChangeLife, so fail is evaluated at the end of a consumed event batch.
    void evaluate_fail();

    const Chart* chart_ = nullptr;
    const JudgmentConstants* constants_ = nullptr;
    bool fail_enabled_ = true;
    std::vector<int> note_row_;     // note_index -> row id (-1 = mine/unscored)
    std::vector<RowAggregate> rows_;
    std::vector<bool> note_scored_; // per-note idempotence guard for tap-like events
    std::vector<bool> hold_scored_; // per-note resolved guard for hold/roll outcomes
    int combo_to_regain_life_ = 0;  // OpenITG m_iComboToRegainLife
    LifeState state_;
};

} // namespace blaze4k
