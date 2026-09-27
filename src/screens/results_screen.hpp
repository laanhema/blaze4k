#pragma once

#include <vector>

#include "screens/results.hpp"
#include "screens/screen.hpp"

namespace td {

// C7 results screen (PRD section 7.3 / section 5 story 6). Reads the finished
// run's ResultsSummary published by GameplayScreen through `ScreenContext::results`,
// submits the best record to the in-memory HighScores (first entry or strictly
// greater percent), and renders grade / percent / DP / per-window judgment
// counts / max combo plus a NEW RECORD or FAILED banner. Confirm (and Back, via
// the manager's default back-navigation) returns to the song wheel.
//
// Pure of SDL/GL/clock: drawing goes through the shared no-op-when-headless
// GlQuadRenderer, and the high-score submit timestamp is read from std::time only
// as display metadata (never on the judgment path).
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

private:
    ResultsSummary summary_{};
    bool new_record_ = false;
    bool submitted_ = false;
};

} // namespace td
