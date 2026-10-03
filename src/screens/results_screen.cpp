#include "screens/results_screen.hpp"

#include <algorithm>
#include <ctime>
#include <iostream>
#include <string>

#include "chart/chart.hpp"
#include "chart/song.hpp"
#include "gameplay/hud_renderer.hpp"
#include "render/bitmap_font.hpp"
#include "render/gl_quad_renderer.hpp"
#include "screens/screen_manager.hpp"
#include "screens/song_display_text.hpp"

namespace blaze4k {

namespace {

// Blaze 4k results palette (presentation, unsourced; no OpenITG parity requirement).
constexpr Color kBackdropColor{0.05f, 0.07f, 0.12f, 1.0f};
constexpr Color kTitleColor{0.86f, 0.93f, 1.00f, 1.0f};
constexpr Color kDimColor{0.55f, 0.60f, 0.72f, 1.0f};
constexpr Color kAccentColor{1.00f, 0.92f, 0.35f, 1.0f};
constexpr Color kHintColor{0.60f, 0.66f, 0.78f, 1.0f};
constexpr Color kFailedColor{1.00f, 0.30f, 0.30f, 1.0f};
constexpr Color kFlashColor{1.00f, 1.00f, 1.00f, 1.0f};

// Tier-colored grade text. Pure presentation: the grade tier itself is already
// pinned by ScoreState/grade_tiers; this only chooses a color.
[[nodiscard]] Color grade_color(double percent) {
    if (percent >= 0.99) {
        return Color{1.00f, 0.85f, 0.25f, 1.0f};
    }
    if (percent >= 0.94) {
        return Color{0.55f, 1.00f, 0.45f, 1.0f};
    }
    if (percent >= 0.80) {
        return Color{0.40f, 0.90f, 1.00f, 1.0f};
    }
    if (percent >= 0.64) {
        return Color{0.86f, 0.93f, 1.00f, 1.0f};
    }
    return Color{0.95f, 0.97f, 1.0f, 1.0f};
}

} // namespace

void ResultsScreen::enter(ScreenContext& ctx) {
    summary_ = ctx.results != nullptr ? *ctx.results : ResultsSummary{};
    new_record_ = false;
    submitted_ = false;

    if (!summary_.valid) {
        animator_.reset(false, false, false);
        std::cout << "[ResultsScreen] no result\n";
        return;
    }

    if (ctx.scores != nullptr) {
        // `submitted_` mirrors eligibility (valid, not failed, fully-formed, percent
        // in range), so it is false when a run cannot be stored despite clearing --
        // e.g. an empty grade label or the Fail-Off all-miss percent < 0 path.
        // `new_record_` is the best-score rule (first entry or strictly greater %).
        submitted_ = results_submit_permitted(summary_);
        new_record_ = results_submit_score(*ctx.scores, summary_,
                                           static_cast<std::int64_t>(std::time(nullptr)));
    }

    animator_.reset(summary_.valid, new_record_, summary_.failed);

    const std::string title = summary_.song != nullptr ? summary_.song->metadata.title : "";
    const std::string difficulty = summary_.chart != nullptr ? summary_.chart->difficulty : "";
    const int meter = summary_.chart != nullptr ? summary_.chart->meter : 0;
    std::cout << "[ResultsScreen] " << title << " " << difficulty << " " << meter << ": "
              << summary_.grade_label << " " << format_percent(summary_.percent) << " DP "
              << summary_.actual_dp << "/" << summary_.possible_dp
              << (summary_.failed ? " FAILED" : " CLEARED") << (new_record_ ? " NEW RECORD" : "")
              << "\n";
}

void ResultsScreen::update(ScreenContext& ctx, double fixed_dt,
                           const std::vector<InputEvent>& events) {
    animator_.update(fixed_dt);

    for (const InputEvent& event : events) {
        if (!event.pressed) {
            continue;
        }
        if (event.action == GameAction::Confirm || event.action == GameAction::Options ||
            event.action == GameAction::Right) {
            // AC3: while the reveal is running a press skips to the final frame
            // without navigating; once it has settled the same press exits to the
            // wheel (C7 behavior). A NO RESULT screen has no reveal, so a press
            // exits immediately -- exactly C7. Back stays manager-owned and exits
            // immediately.
            if (has_reveal() && !animator_.finished()) {
                animator_.skip();
                return;
            }
            if (ctx.manager != nullptr) {
                ctx.manager->transition_to(ScreenId::Select);
            }
            return;
        }
    }
}

void ResultsScreen::render(ScreenContext& /*ctx*/, GlQuadRenderer& renderer, int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }

    const float width = static_cast<float>(w);
    const float height = static_cast<float>(h);
    renderer.draw_quad(Rect{0.0f, 0.0f, width, height}, kBackdropColor);

    draw_text(renderer, "RESULTS", width * 0.05f, height * 0.06f, std::max(2.5f, width * 0.0045f),
              kTitleColor);

    if (!has_reveal()) {
        draw_text_centered(renderer, "NO RESULT", width * 0.5f, height * 0.42f,
                           std::max(3.0f, width * 0.006f), kDimColor);
        draw_text_centered(renderer, "[ENTER] CONTINUE", width * 0.5f, height * 0.85f, 2.0f,
                           kHintColor);
        return;
    }

    const double elapsed = animator_.elapsed();
    const float title_alpha = ResultsAnimator::title_alpha(elapsed);
    const float stats_alpha = ResultsAnimator::stats_alpha(elapsed);

    // Full-screen white flash behind the text, only on a NEW RECORD finale.
    if (shows_record_finale()) {
        const float flash = ResultsAnimator::record_flash(elapsed);
        if (flash > 0.0f) {
            renderer.draw_quad(Rect{0.0f, 0.0f, width, height},
                               with_alpha(kFlashColor, flash));
        }
    }

    const std::string title =
        summary_.song != nullptr ? song_display_title(summary_.song->metadata) : "UNKNOWN";
    const std::string artist =
        summary_.song != nullptr ? song_display_artist(summary_.song->metadata) : "";
    draw_text_centered(renderer, title, width * 0.5f, height * 0.12f,
                       std::max(2.5f, width * 0.004f), with_alpha(kTitleColor, title_alpha));
    if (!artist.empty()) {
        draw_text_centered(renderer, artist, width * 0.5f, height * 0.19f, 2.0f,
                           with_alpha(kDimColor, title_alpha));
    }

    const std::string diff_line =
        summary_.chart != nullptr
            ? summary_.chart->difficulty + " " + std::to_string(summary_.chart->meter)
            : std::string("UNKNOWN");
    draw_text_centered(renderer, diff_line, width * 0.5f, height * 0.25f, 2.0f,
                       with_alpha(kHintColor, title_alpha));

    // Large, tier-colored grade: fades in as it slams from 2.4x to 1.0x.
    const float grade_alpha = ResultsAnimator::grade_alpha(elapsed);
    if (grade_alpha > 0.0f) {
        const GradeTier tier{0.0, summary_.grade_label.c_str()};
        draw_text_centered(renderer, format_grade(tier), width * 0.5f, height * 0.31f,
                           std::max(4.0f, width * 0.008f) *
                               ResultsAnimator::grade_scale(elapsed),
                           with_alpha(grade_color(summary_.percent), grade_alpha));
    }

    // Percent counts up to (and never past) the exact C7 value.
    draw_text_centered(renderer,
                       format_percent(summary_.percent *
                                      ResultsAnimator::percent_progress(elapsed)),
                       width * 0.5f, height * 0.44f, 3.0f, kAccentColor);
    draw_text_centered(renderer, "DP " + std::to_string(summary_.actual_dp) + "/" +
                                     std::to_string(summary_.possible_dp),
                       width * 0.5f, height * 0.50f, 2.5f, with_alpha(kDimColor, stats_alpha));
    draw_text_centered(renderer, "MAX COMBO " + std::to_string(summary_.max_combo), width * 0.5f,
                       height * 0.55f, 2.5f, with_alpha(kDimColor, stats_alpha));

    const auto tap_count = [this](TapJudgment j) {
        return summary_.tap_counts[static_cast<std::size_t>(j)];
    };
    const std::string windows =
        "F " + std::to_string(tap_count(TapJudgment::Fantastic)) + "  E " +
        std::to_string(tap_count(TapJudgment::Excellent)) + "  G " +
        std::to_string(tap_count(TapJudgment::Great)) + "  D " +
        std::to_string(tap_count(TapJudgment::Decent)) + "  W " +
        std::to_string(tap_count(TapJudgment::WayOff)) + "  M " +
        std::to_string(tap_count(TapJudgment::Miss)) + "  MINE " +
        std::to_string(tap_count(TapJudgment::HitMine));
    draw_text_centered(renderer, windows, width * 0.5f, height * 0.62f, 2.0f,
                       with_alpha(kDimColor, stats_alpha));

    const std::string holds =
        "HOLD OK " +
        std::to_string(summary_.hold_counts[static_cast<std::size_t>(HoldJudgment::Ok)]) +
        "  NG " + std::to_string(summary_.hold_counts[static_cast<std::size_t>(HoldJudgment::Ng)]);
    draw_text_centered(renderer, holds, width * 0.5f, height * 0.67f, 2.0f,
                       with_alpha(kDimColor, stats_alpha));

    const float hint_scale = std::max(3.0f, width * 0.006f);
    if (shows_record_finale()) {
        const float record_alpha = ResultsAnimator::record_alpha(elapsed);
        if (record_alpha > 0.0f) {
            draw_text_centered(renderer, "NEW RECORD", width * 0.5f, height * 0.75f,
                               hint_scale * ResultsAnimator::record_scale(elapsed),
                               with_alpha(kAccentColor, record_alpha));
        }
    } else if (summary_.failed) {
        draw_text_centered(renderer, "FAILED", width * 0.5f, height * 0.75f, hint_scale,
                           with_alpha(kFailedColor, ResultsAnimator::failed_alpha(elapsed)));
    }

    if (!animator_.finished()) {
        draw_text_centered(renderer, "[ENTER] SKIP", width * 0.5f, height * 0.87f, 2.0f, kHintColor);
    } else if (summary_.failed) {
        draw_text_centered(renderer, "[ENTER] RETURN TO WHEEL", width * 0.5f, height * 0.87f, 2.0f,
                           kHintColor);
    } else {
        draw_text_centered(renderer, "[ENTER] CONTINUE", width * 0.5f, height * 0.87f, 2.0f,
                           kHintColor);
    }
}

} // namespace blaze4k
