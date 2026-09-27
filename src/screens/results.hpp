#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "data/high_scores.hpp"
#include "gameplay/score_keeper.hpp"

namespace td {

// Pure, SDL/GL/audio/clock-free snapshot of one finished run (mirrors the
// OptionsMenu / InputRemapModel seam). `GameplayScreen` copies the already
// event-sourced ScoreState into this flat value on run end, so ResultsScreen
// never holds live GameplayView state and the whole model stays
// headless-testable. No <ctime>: the timestamp is injected by the caller.
struct ResultsSummary {
    bool valid = false;
    const Song* song = nullptr;   // SongLibrary-owned; null-safe
    const Chart* chart = nullptr; // SongLibrary-owned; null-safe
    bool failed = false;          // GameplayOutcome::Failed (life empty, fail-enabled)
    std::string grade_label;      // GradeTier::label from state.grade ("" if null)
    double percent = 0.0;         // ScoreState::percent, unclamped
    int actual_dp = 0;            // ScoreState::actual_dp
    int possible_dp = 0;          // ScoreState::possible_dp
    int max_combo = 0;            // ScoreState::max_combo
    std::array<int, static_cast<std::size_t>(TapJudgment::Num)> tap_counts{};
    std::array<int, static_cast<std::size_t>(HoldJudgment::Num)> hold_counts{};
};

// Copies a live ScoreState plus the life fail flag into a flat summary.
[[nodiscard]] ResultsSummary results_summary_from(const ScoreState& state, bool failed,
                                                  const Song* song, const Chart* chart);

// True when a run is eligible to become a stored best record: valid, not failed,
// song/chart present, a non-empty grade label, and a finite percent in [0, 1].
// Shared by results_submit_score's guard and ResultsScreen, so the screen's
// `submitted` flag reports actual eligibility rather than merely `!failed`.
[[nodiscard]] bool results_submit_permitted(const ResultsSummary& summary);

// Submits the run's best record and returns true when it is a new personal best
// (first entry for the chart, or a strictly greater percent). No-op (false) for
// runs that are not permitted (see results_submit_permitted). The timestamp is
// injected so this stays <ctime>-free.
[[nodiscard]] bool results_submit_score(HighScores& scores, const ResultsSummary& summary,
                                        std::int64_t timestamp_unix);

} // namespace td
