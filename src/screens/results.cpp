#include "screens/results.hpp"

#include <cmath>

namespace td {

ResultsSummary results_summary_from(const ScoreState& state, bool failed, const Song* song,
                                    const Chart* chart) {
    ResultsSummary summary;
    summary.valid = true;
    summary.song = song;
    summary.chart = chart;
    summary.failed = failed;
    summary.grade_label = state.grade != nullptr ? state.grade->label : "";
    summary.percent = state.percent;
    summary.actual_dp = state.actual_dp;
    summary.possible_dp = state.possible_dp;
    summary.max_combo = state.max_combo;
    summary.tap_counts = state.tap_counts;
    summary.hold_counts = state.hold_counts;
    return summary;
}

bool results_submit_permitted(const ResultsSummary& summary) {
    // Percent is deliberately bounded on both ends and never clamped. A completed
    // Fail-Off run (config fail_enabled=false) that misses every note computes a
    // negative ScoreState::percent (the implementation report measured -2.4), and
    // the on-disk loader (high_scores.cpp) drops records outside [0, 1] on load.
    // Storing such a run would create a record that silently vanishes on the next
    // startup, so it is intentionally not recorded -- the Results screen still
    // shows its stats, it just cannot become a best record (and so never flags
    // NEW RECORD). This mirrors the plan's pinned Task 1 guard. The upper bound is
    // enforced for symmetry with the loader; recompute_derived already clamps an
    // exact all-perfect clear to 1.0, so it is unreachable in practice.
    return summary.valid && !summary.failed && summary.song != nullptr &&
           summary.chart != nullptr && !summary.grade_label.empty() &&
           std::isfinite(summary.percent) && summary.percent >= 0.0 &&
           summary.percent <= 1.0;
}

bool results_submit_score(HighScores& scores, const ResultsSummary& summary,
                          std::int64_t timestamp_unix) {
    if (!results_submit_permitted(summary)) {
        return false;
    }

    ScoreRecord record;
    record.grade = summary.grade_label;
    record.percent = summary.percent;
    record.dance_points = summary.actual_dp;
    record.timestamp_unix = timestamp_unix;

    return submit_high_score(scores, make_chart_key(*summary.song, *summary.chart), record);
}

} // namespace td
