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

// Cabinet v3 score screen (#95). Reads ResultsSummary and submits high scores.
//
// Layout (results_art, cabinet-v3-results.png): bg_results; bar_top with the
// title_score_screen sprite and the difficulty badge, song title and artist
// right-aligned; three stat_panel plates (MAX COMBO, DANCE POINTS "n / max",
// HOLDS OK / NG / MINES) in digits_white; the medallion with grade_<tier> and the
// tier label; the digits_chrome percentage; a record_ribbon (NEW RECORD) or
// failed_ribbon; six judgment rows with code-drawn bars; the 44px hint bar
// ("ENTER SKIP" during the reveal, "ENTER CONTINUE" once settled); scanlines.
class ResultsScreen : public Screen {
public:
    [[nodiscard]] ScreenId id() const override { return ScreenId::Results; }

    void enter(ScreenContext& ctx) override;
    void update(ScreenContext& ctx, double fixed_dt, const std::vector<InputEvent>& events) override;
    void render(ScreenContext& ctx, GlQuadRenderer& renderer, int w, int h) override;

    [[nodiscard]] const ResultsSummary& summary() const { return summary_; }
    [[nodiscard]] bool new_record() const { return new_record_; }
    // True only when the run was eligible to be stored (results_submit_permitted);
    // false for failed or otherwise unstorable runs.
    [[nodiscard]] bool submitted() const { return submitted_; }
    [[nodiscard]] bool valid() const { return summary_.valid; }
    [[nodiscard]] const ResultsAnimator& animator() const { return animator_; }
    [[nodiscard]] bool reveal_finished() const { return animator_.finished(); }
    [[nodiscard]] std::string_view hint_word() const {
        return has_reveal() && !animator_.finished() ? "SKIP" : "CONTINUE";
    }
    [[nodiscard]] bool shows_record_finale() const {
        return summary_.valid && animator_.new_record();
    }

    [[nodiscard]] const DifficultyBadge& badge() const { return badge_; }
    [[nodiscard]] const std::string& display_title() const { return title_; }
    [[nodiscard]] const std::string& display_artist() const { return artist_; }
    [[nodiscard]] const std::string& display_subtitle() const { return subtitle_; }
    [[nodiscard]] const std::string& fitted_subtitle() const { return fitted_subtitle_; }
    [[nodiscard]] const std::string& grade_texture() const { return grade_texture_; }
    [[nodiscard]] const std::string& tier_text() const { return tier_text_; }
    [[nodiscard]] const std::string& max_combo_text() const { return max_combo_text_; }
    [[nodiscard]] const std::string& dp_text() const { return dp_text_; }
    [[nodiscard]] const std::string& dp_max_text() const { return dp_max_text_; }
    [[nodiscard]] const std::array<std::string, 3>& hold_texts() const { return hold_texts_; }
    [[nodiscard]] const std::array<std::string, results_art::kJudgmentRowCount>& count_texts() const {
        return count_texts_;
    }

private:
    [[nodiscard]] bool has_reveal() const { return summary_.valid; }

    void clear_cached_text();
    void build_cached_text(const ScreenContext& ctx);
    void refit_bar_text(const TextRenderer* text, const ThemeTextures* theme);

    ResultsSummary summary_{};
    bool new_record_ = false;
    bool submitted_ = false;
    ResultsAnimator animator_{};

    DifficultyBadge badge_{};
    std::string title_;
    std::string subtitle_;
    std::string artist_;
    std::string grade_texture_;
    std::string grade_fallback_text_;
    std::string tier_text_;
    std::string max_combo_text_;
    std::string dp_text_;
    std::string dp_max_text_;
    std::array<std::string, 3> hold_texts_{};
    std::array<std::string, results_art::kJudgmentRowCount> count_texts_{};

    float fitted_scale_ = -1.0f;
    results_art::TopBarLayout bar_layout_{};
    std::array<float, 3> hold_cols_{};
    std::string fitted_badge_;
    std::string fitted_title_;
    std::string fitted_subtitle_;
    float subtitle_x_ = 0.0f;
    std::string fitted_artist_;
};

} // namespace blaze4k
