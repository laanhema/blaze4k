#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

#include "chart/chart.hpp"
#include "gameplay/judgment.hpp"
#include "timing/judgment_constants.hpp"

namespace blaze4k {

// Live score derived solely from the B4 judgment log (PRD section 6 pattern 2).
//
// Pure value type: no clocks, no platform headers. Every field is recomputed from
// the append-only `JudgmentEvent` stream, exactly like OpenITG's `ScoreKeeperMAX2`
// derives its counters from judged note data (never from frame timing).
struct ScoreState {
    int actual_dp = 0;   // OpenITG iActualDancePoints (may go negative, except on MercifulBeginner)
    int possible_dp = 0; // chart-derived maximum, computed once in reset()
    int combo = 0;
    int max_combo = 0;
    int miss_combo = 0; // OpenITG iCurMissCombo
    std::array<int, static_cast<std::size_t>(TapJudgment::Num)> tap_counts{};
    std::array<int, static_cast<std::size_t>(HoldJudgment::Num)> hold_counts{};
    double percent = 0.0;             // unclamped; may be negative
    const GradeTier* grade = nullptr; // points into constants.grade_tiers
};

// Pure, event-sourced scorer.
//
// Consumes the B4 `JudgmentEvent` log and aggregates per-note events into OpenITG
// rows (row identity = exact `Note.beat`). It includes no platform, audio, or
// time headers, so the scoring path can never consult wall-clock or frame timing
// (AGENTS.md core principle 1).
//
// It owns score/combo/percent/grade and nothing else: no life or fail handling.
class ScoreKeeper {
public:
    // `chart` and `constants` must outlive the keeper. Builds the chart-derived
    // row tables and possible-DP denominator; clears all score state.
    // Precondition: `chart->notes` is beat-sorted with equal beats contiguous
    // (the NoteParser guarantee), which is what lets reset() group rows by beat.
    void reset(const Chart* chart, const JudgmentConstants* constants);

    // Consume one immutable judgment event (append-only log; idempotent per note).
    void consume(const JudgmentEvent& event);
    void consume(const std::vector<JudgmentEvent>& events);

    [[nodiscard]] const ScoreState& state() const { return state_; }
    [[nodiscard]] int actual_dance_points() const { return state_.actual_dp; }
    [[nodiscard]] int possible_dance_points() const { return state_.possible_dp; }
    [[nodiscard]] double percent() const { return state_.percent; }
    [[nodiscard]] const GradeTier& grade() const; // constants.grade_for_percent(percent)
    [[nodiscard]] bool is_complete() const;       // every row resolved and every hold scored

private:
    struct RowAggregate {
        int expected = 0; // tap/hold-head notes in this row
        int judged = 0;
        bool has_miss = false;
        double last_delta_ms = 0.0; // greatest offset wins; ties -> later column
        int last_column = -1;
        TapJudgment last_window = TapJudgment::Num;
    };

    void apply_tap_like(const JudgmentEvent& event, bool miss);
    void apply_hold(const JudgmentEvent& event);
    void resolve_row(int row);
    void recompute_derived();
    [[nodiscard]] int tap_weight(TapJudgment j) const;
    // OpenITG MercifulBeginner: on a Beginner chart negative DP weights count as
    // 0 (ScoreKeeperMAX2.cpp:529-530 tap/mine, :544-545 hold). Upstream excludes
    // course mode (:187-189), which Blaze does not have. possible_dp is never
    // clamped (:449-451 pass false). Blaze derives grade from DP percent, so this
    // also covers the grade-weight clamp (:570-571, :585-586;
    // PlayerStageStats.cpp:164-179); if grade weights ever get their own
    // accumulation, apply the same clamp there.
    [[nodiscard]] int dp_weight(int weight) const { return merciful_ ? std::max(0, weight) : weight; }

    const Chart* chart_ = nullptr;
    const JudgmentConstants* constants_ = nullptr;
    bool merciful_ = false; // MercifulBeginner rules apply to this chart
    std::vector<int> note_row_;     // note_index -> row id (-1 = mine/unscored)
    std::vector<RowAggregate> rows_;
    std::vector<bool> note_scored_; // per-note idempotence guard for tap-like events
    std::vector<bool> hold_scored_; // per-note resolved guard (outcome or missed head)
    ScoreState state_;
};

} // namespace blaze4k
