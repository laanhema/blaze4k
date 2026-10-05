#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "gameplay/hud_renderer.hpp"

#include "screens/results.hpp"
#include "screens/results_anim.hpp"
#include "screens/results_art.hpp"
#include "screens/screen.hpp"

namespace blaze4k {

// Cabinet v3 score screen (#95; C7/D3 results, PRD section 7.3 / section 5 story 6,
// section 12 Phase D). Reads the finished run's ResultsSummary published by
// GameplayScreen through `ScreenContext::results`, submits the best record to the
// in-memory HighScores (first entry or strictly greater percent), exactly as C7.
//
// Layout (results_art, cabinet-v3-results.png): bg_results; bar_top with the
// title_score_screen sprite and the difficulty badge, song title and artist
// right-aligned; three stat_panel plates (MAX COMBO, DANCE POINTS "n / max",
// HOLDS OK / NG / MINES) in digits_white; the medallion with grade_<tier> and the
// tier label; the digits_chrome percentage; a record_ribbon (NEW RECORD) or
// failed_ribbon; six judgment rows with code-drawn bars; the 44px hint bar
// ("ENTER SKIP" during the reveal, "ENTER CONTINUE" once settled); scanlines.
//
// The D3 reveal (ResultsAnimator, curves unchanged) drives the Cabinet parts:
// the bar text fades in (title_alpha), the grade slams 2.4x -> 1x on the
// medallion (grade_scale / grade_alpha), the percent counts up
// (percent_progress), the panels and judgment rows fade in (stats_alpha), the
// NEW RECORD ribbon punches and pulses (record_scale / record_alpha) over a
// white flash (record_flash), and the FAILED ribbon fades in (failed_alpha). A
// press during the reveal skips to the settled frame; once finished Confirm
// returns to the song wheel (Back exits immediately via the manager's default
// navigation).
//
// Every string is built in enter(); the bar's fitted strings are re-fitted only
// when the text scale changes. Null theme / text services draw nothing
// (headless). Pure of SDL/clock: drawing goes through the shared
// no-op-when-headless GlQuadRenderer, the reveal is advanced only by `fixed_dt`,
// and the high-score submit timestamp is read from std::time only as display
// metadata (never on the judgment path).
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
    // The hint-bar word after "ENTER" (owner decision 6): "SKIP" while the reveal
    // runs, "CONTINUE" once it has settled or on a NO RESULT screen (no reveal).
    [[nodiscard]] std::string_view hint_word() const {
        return has_reveal() && !animator_.finished() ? "SKIP" : "CONTINUE";
    }
    // Test accessor: the single predicate render() uses to gate every NEW RECORD
    // draw (accent flash + banner); false for a normal clear, a sub-best run, a
    // failed run, or an invalid summary.
    [[nodiscard]] bool shows_record_finale() const {
        return summary_.valid && animator_.new_record();
    }

    // Test accessors: the strings cached by enter() (empty for an invalid summary).
    [[nodiscard]] const DifficultyBadge& badge() const { return badge_; }
    [[nodiscard]] const std::string& display_title() const { return title_; }
    [[nodiscard]] const std::string& display_artist() const { return artist_; }
    // The song's display subtitle (#110; empty when it has none) and the
    // subtitle as fitted to the top bar by the last render (empty without a
    // text service or when it got no room).
    [[nodiscard]] const std::string& display_subtitle() const { return subtitle_; }
    [[nodiscard]] const std::string& fitted_subtitle() const { return fitted_subtitle_; }
    [[nodiscard]] const std::string& grade_texture() const { return grade_texture_; }
    [[nodiscard]] const std::string& tier_text() const { return tier_text_; }
    [[nodiscard]] const std::string& max_combo_text() const { return max_combo_text_; }
    [[nodiscard]] const std::string& dp_text() const { return dp_text_; }
    [[nodiscard]] const std::string& dp_max_text() const { return dp_max_text_; }
    // HOLDS OK, NG, MINES (mines stepped on).
    [[nodiscard]] const std::array<std::string, 3>& hold_texts() const { return hold_texts_; }
    // FANTASTIC..MISS tap counts.
    [[nodiscard]] const std::array<std::string, results_art::kJudgmentRowCount>& count_texts() const {
        return count_texts_;
    }

private:
    // The animated reveal exists only for a valid summary. A NO RESULT screen has
    // no reveal: render() draws its static CONTINUE hint immediately and update()
    // must navigate immediately (exactly C7). Both consult this one helper so the
    // render and input paths cannot drift.
    [[nodiscard]] bool has_reveal() const { return summary_.valid; }

    void clear_cached_text();
    void build_cached_text(const ScreenContext& ctx);
    // Re-fits the badge / title / subtitle / artist to the bar, and the holds panel
    // columns, when the text scale changed (every call without a text service).
    void refit_bar_text(const TextRenderer* text, const ThemeTextures* theme);

    ResultsSummary summary_{};
    bool new_record_ = false;
    bool submitted_ = false;
    ResultsAnimator animator_{};

    // Built once in enter().
    DifficultyBadge badge_{};
    std::string title_;
    std::string subtitle_;
    std::string artist_;
    std::string grade_texture_;
    std::string grade_fallback_text_; // format_grade, drawn when the sprite is missing
    std::string tier_text_;
    std::string max_combo_text_;
    std::string dp_text_;
    std::string dp_max_text_;
    std::array<std::string, 3> hold_texts_{};
    std::array<std::string, results_art::kJudgmentRowCount> count_texts_{};

    // Top bar fit cache: refreshed when the text scale changes (fitted_scale_ < 0
    // forces a refit after enter()).
    float fitted_scale_ = -1.0f;
    results_art::TopBarLayout bar_layout_{};
    std::array<float, 3> hold_cols_{}; // holds panel column offsets (reference px)
    std::string fitted_badge_;
    std::string fitted_title_;
    std::string fitted_subtitle_;
    float subtitle_x_ = 0.0f; // reference px: after the measured fitted title + kBarSubtitleGap
    std::string fitted_artist_;
};

} // namespace blaze4k
