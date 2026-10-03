#pragma once

#include <string>
#include <vector>

#include "screens/results.hpp"
#include "screens/results_anim.hpp"
#include "screens/screen.hpp"

namespace blaze4k {

// Pure results-screen difficulty line: "UNKNOWN" for a null chart, else the
// display label (chart_display_label: an Edit chart's name, else the passthrough
// label) + " " + meter. The name is shortened so the line fits
// `max_line_width` pixels at `pixel` scale, but never below 9 cells (the width
// of "Challenge").
[[nodiscard]] std::string results_difficulty_line(const Chart* chart, float max_line_width,
                                                  float pixel);

// C7/D3 results screen (PRD section 7.3 / section 5 story 6, section 12 Phase D).
// Reads the finished run's ResultsSummary published by GameplayScreen through
// `ScreenContext::results`, submits the best record to the in-memory HighScores
// (first entry or strictly greater percent), and renders grade / percent / DP /
// per-window judgment counts / max combo plus a NEW RECORD or FAILED banner. D3
// adds a presentational arcade reveal (grade slam, percent count-up, fading rows,
// NEW RECORD punch/pulse/flash) driven by a pure ResultsAnimator; a press during
// the reveal skips to the final frame, and once finished Confirm returns to the
// song wheel (Back exits immediately via the manager's default navigation).
//
// Pure of SDL/GL/clock: drawing goes through the shared no-op-when-headless
// GlQuadRenderer, the reveal is advanced only by `fixed_dt`, and the high-score
// submit timestamp is read from std::time only as display metadata (never on the
// judgment path).
class ResultsScreen : public Screen {
public:
    [[nodiscard]] ScreenId id() const override { return ScreenId::Results; }

    void enter(ScreenContext& ctx) override;
    void update(ScreenContext& ctx, double fixed_dt, const std::vector<InputEvent>& events) override;
    void render(ScreenContext& ctx, GlQuadRenderer& renderer, int w, int h) override;

    // Test accessors (headless, pure).
    [[nodiscard]] const ResultsSummary& summary() const { return summary_; }
    [[nodiscard]] bool new_record() const { return new_record_; }
    // True only when the run was eligible to be stored (results_submit_permitted);
    // false for failed or otherwise unstorable runs.
    [[nodiscard]] bool submitted() const { return submitted_; }
    [[nodiscard]] bool valid() const { return summary_.valid; }
    [[nodiscard]] const ResultsAnimator& animator() const { return animator_; }
    [[nodiscard]] bool reveal_finished() const { return animator_.finished(); }
    // Test accessor: the single predicate render() uses to gate every NEW RECORD
    // draw (accent flash + banner); false for a normal clear, a sub-best run, a
    // failed run, or an invalid summary.
    [[nodiscard]] bool shows_record_finale() const {
        return summary_.valid && animator_.new_record();
    }

private:
    // The animated reveal exists only for a valid summary. A NO RESULT screen has
    // no reveal: render() draws its static CONTINUE hint immediately and update()
    // must navigate immediately (exactly C7). Both consult this one helper so the
    // render and input paths cannot drift.
    [[nodiscard]] bool has_reveal() const { return summary_.valid; }

    ResultsSummary summary_{};
    bool new_record_ = false;
    bool submitted_ = false;
    ResultsAnimator animator_{};
};

} // namespace blaze4k
