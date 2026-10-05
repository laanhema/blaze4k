#include "screens/results_screen.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <ctime>
#include <iostream>
#include <string>
#include <string_view>

#include "chart/chart.hpp"
#include "chart/song.hpp"
#include "gameplay/hud_renderer.hpp"
#include "gameplay/score_keeper.hpp"
#include "render/gl_quad_renderer.hpp"
#include "render/theme_textures.hpp"
#include "render/ttf_font.hpp"
#include "screens/gameplay_screen.hpp"
#include "screens/screen_manager.hpp"
#include "screens/song_display_text.hpp"

namespace blaze4k {

namespace {

namespace layout = theme::layout;
namespace art = results_art;

// Holds panel labels: HOLDS OK, NG, MINES.
constexpr std::array<std::string_view, 3> kHoldLabels = {"HOLDS OK", "NG", "MINES"};

// Line-box top for a reference baseline.
float baseline_top(const TextRenderer& text, const theme::LayoutScale& L, float ref_baseline,
                   const theme::TextStyle& style) {
    return L.y(ref_baseline) - text.ascent(style);
}

// Glyph-rect top of a digits_white line drawn at `scale` with its baseline at `ref_baseline`.
float digit_top(float ref_baseline, float scale) {
    return ref_baseline - art::kDigitBaselineRef * scale;
}

} // namespace

void ResultsScreen::enter(ScreenContext& ctx) {
    summary_ = ctx.results != nullptr ? *ctx.results : ResultsSummary{};
    new_record_ = false;
    submitted_ = false;

    clear_cached_text();

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

    build_cached_text(ctx);
}

void ResultsScreen::clear_cached_text() {
    badge_ = DifficultyBadge{};
    title_.clear();
    subtitle_.clear();
    artist_.clear();
    grade_texture_.clear();
    grade_fallback_text_.clear();
    tier_text_.clear();
    max_combo_text_.clear();
    dp_text_.clear();
    dp_max_text_.clear();
    for (std::string& text : hold_texts_) {
        text.clear();
    }
    for (std::string& text : count_texts_) {
        text.clear();
    }
    fitted_scale_ = -1.0f;
    bar_layout_ = art::TopBarLayout{};
    hold_cols_ = {};
    fitted_badge_.clear();
    fitted_title_.clear();
    fitted_subtitle_.clear();
    subtitle_x_ = 0.0f;
    fitted_artist_.clear();
}

void ResultsScreen::build_cached_text(const ScreenContext& ctx) {
    badge_ = summary_.chart != nullptr ? difficulty_badge_for(*summary_.chart) : DifficultyBadge{};
    title_ = summary_.song != nullptr
                 ? song_display_title(summary_.song->metadata, ctx.text,
                                      theme::text::kBarSongTitle.font)
                 : std::string{"UNKNOWN"};
    subtitle_ = summary_.song != nullptr
                    ? song_display_subtitle(summary_.song->metadata, ctx.text,
                                            theme::text::kBarSongSubtitle.font)
                    : std::string{};
    artist_ = summary_.song != nullptr
                  ? song_display_artist(summary_.song->metadata, ctx.text,
                                        theme::text::kBarArtist.font)
                  : std::string{};

    grade_texture_ = art::grade_texture_name(summary_.grade_label);
    grade_fallback_text_ = format_grade(GradeTier{0.0, summary_.grade_label.c_str()});
    tier_text_ = art::grade_tier_text(summary_.grade_label);

    // Every digit string comes from digits_text (never negative), so only
    // kDigitChars reach BitmapDigits.
    max_combo_text_ = art::digits_text(summary_.max_combo);
    dp_text_ = art::digits_text(summary_.actual_dp);
    dp_max_text_ = "/ " + art::digits_text(summary_.possible_dp);
    hold_texts_ = {
        art::digits_text(summary_.hold_counts[static_cast<std::size_t>(HoldJudgment::Ok)]),
        art::digits_text(summary_.hold_counts[static_cast<std::size_t>(HoldJudgment::Ng)]),
        art::digits_text(summary_.tap_counts[static_cast<std::size_t>(TapJudgment::HitMine)]),
    };
    for (std::size_t i = 0; i < count_texts_.size(); ++i) {
        count_texts_[i] = std::to_string(summary_.tap_counts[i]);
    }
    fitted_scale_ = -1.0f;
}

void ResultsScreen::refit_bar_text(const TextRenderer* text, const ThemeTextures* theme) {
    // Holds panel column offsets (reference px) from the label and value widths.
    const auto fit_hold_columns = [this, text, theme]() {
        std::array<float, 3> label_w{};
        std::array<float, 3> value_w{};
        for (std::size_t i = 0; i < 3; ++i) {
            if (text != nullptr) {
                label_w[i] = ref_measure(*text, kHoldLabels[i], theme::text::kStatLabel);
            }
            if (theme != nullptr) {
                value_w[i] = theme->digits_white().measure(hold_texts_[i], art::kHoldScale);
            }
        }
        hold_cols_ = art::hold_columns(label_w, value_w);
    };
    if (text == nullptr) {
        // No text service: no widths, so no plate and no text.
        bar_layout_ = art::top_bar_layout(0.0f, 0.0f, 0.0f);
        fitted_badge_.clear();
        fitted_title_.clear();
        fitted_subtitle_.clear();
        subtitle_x_ = 0.0f;
        fitted_artist_.clear();
        fitted_scale_ = -1.0f;
        fit_hold_columns();
        return;
    }
    if (text->scale() == fitted_scale_) {
        return;
    }
    fitted_scale_ = text->scale();
    fit_hold_columns();
    const float scale = fitted_scale_ > 0.0f ? fitted_scale_ : 1.0f;
    const auto measure_badge = [text](std::string_view s) {
        return text->measure(s, theme::text::kBarBadge);
    };
    const float badge_w = badge_text_width(badge_, measure_badge) / scale;
    const float subtitle_w =
        subtitle_.empty() ? 0.0f : ref_measure(*text, subtitle_, theme::text::kBarSongSubtitle);
    bar_layout_ = art::top_bar_layout(badge_w,
                                      ref_measure(*text, title_, theme::text::kBarSongTitle),
                                      ref_measure(*text, artist_, theme::text::kBarArtist),
                                      subtitle_w);
    fitted_badge_ = bar_layout_.plate.w > 0.0f
                        ? fit_badge_text(badge_, bar_layout_.badge_text_max_w * scale, measure_badge)
                        : std::string{};
    fitted_title_ =
        text->truncate(title_, theme::text::kBarSongTitle, bar_layout_.title_max_w * scale);
    // The subtitle follows the measured fitted title, so the gap stays tight after truncation.
    fitted_subtitle_ = bar_layout_.subtitle_max_w > 0.0f
                           ? text->truncate(subtitle_, theme::text::kBarSongSubtitle,
                                            bar_layout_.subtitle_max_w * scale)
                           : std::string{};
    subtitle_x_ = bar_layout_.title_x +
                  ref_measure(*text, fitted_title_, theme::text::kBarSongTitle) +
                  art::kBarSubtitleGap;
    fitted_artist_ =
        text->truncate(artist_, theme::text::kBarArtist, bar_layout_.artist_max_w * scale);
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

void ResultsScreen::render(ScreenContext& ctx, GlQuadRenderer& renderer, int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }

    // Cabinet v3 score screen (#95), back to front. Every theme/text call is
    // null-guarded (headless and unit tests run without them).
    const theme::LayoutScale L = theme::layout_scale(w, h);
    const ThemeTextures* theme = ctx.theme;
    TextRenderer* text = ctx.text;
    const Rect window{0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h)};

    // 1. Background, then the NEW RECORD flash behind everything else.
    if (theme != nullptr) {
        art::draw_backdrop(*theme, renderer, w, h);
    }
    const double elapsed = animator_.elapsed();
    if (shows_record_finale()) {
        const float flash = ResultsAnimator::record_flash(elapsed);
        if (flash > 0.0f) {
            renderer.draw_quad(window, Color{1.0f, 1.0f, 1.0f, flash});
        }
    }

    // 2. Top bar (+ title sprite) and hint bar.
    art::draw_bars(theme, text, renderer, L, w);

    // 3. NO RESULT: no reveal, the CONTINUE hint shows at once.
    if (!has_reveal()) {
        if (text != nullptr) {
            text->draw(renderer, "NO RESULT", L.x(art::kHintCentreX), L.y(art::kEmptyMessageTop),
                       theme::text::kWheelRow, TextAlign::Centre);
            art::draw_hint_text(*text, renderer, L, hint_word());
        }
        if (theme != nullptr) {
            art::draw_scanlines(*theme, renderer, w, h, L);
        }
        return;
    }

    refit_bar_text(text, theme);

    const float title_alpha = ResultsAnimator::title_alpha(elapsed);
    const float grade_alpha = ResultsAnimator::grade_alpha(elapsed);
    const float stats_alpha = ResultsAnimator::stats_alpha(elapsed);
    const art::TopBarLayout& bar = bar_layout_;

    // 4. Plates and bars: medallion, stat panels, judgment tracks + fills, badge plate.
    if (theme != nullptr) {
        theme->draw_sprite(renderer, "medallion", L.point(Vec2{layout::kMedallion.x,
                                                               layout::kMedallion.y}),
                           L.s);
        art::draw_stat_panel_plates(*theme, renderer, L, stats_alpha);
    }
    art::draw_judgment_bars(renderer, L, summary_, stats_alpha);
    if (bar.plate.w > 0.0f && title_alpha > 0.0f) {
        art::draw_skewed_solid(renderer, L, bar.plate, theme::skew::kRows,
                               with_alpha(badge_.colors.fill, badge_.colors.fill.a * title_alpha));
    }

    // 5. Sprites: the grade slams onto the medallion; then the ribbon.
    const bool grade_sprite = theme != nullptr && !grade_texture_.empty() &&
                              theme->entry(grade_texture_) != nullptr;
    if (grade_sprite && grade_alpha > 0.0f) {
        const float scale = ResultsAnimator::grade_scale(elapsed);
        const Vec2 content_ref = theme->content_size(grade_texture_, 1.0f);
        const Vec2 content =
            (content_ref.x > 0.0f && content_ref.y > 0.0f) ? content_ref : art::kGradeContentRef;
        const Rect rect = art::grade_rect(content, scale);
        theme->draw_sprite(renderer, grade_texture_, L.point(Vec2{rect.x, rect.y}), L.s * scale,
                           Color{1.0f, 1.0f, 1.0f, grade_alpha});
    }
    if (theme != nullptr) {
        if (shows_record_finale()) {
            const float scale = ResultsAnimator::record_scale(elapsed);
            const float alpha = ResultsAnimator::record_alpha(elapsed);
            if (scale > 0.0f && alpha > 0.0f) {
                const Rect rect = art::ribbon_rect(scale);
                theme->draw_sprite(renderer, "record_ribbon", L.point(Vec2{rect.x, rect.y}),
                                   L.s * scale, Color{1.0f, 1.0f, 1.0f, alpha});
            }
        } else if (summary_.failed) {
            const float alpha = ResultsAnimator::failed_alpha(elapsed);
            if (alpha > 0.0f) {
                const Rect rect = art::ribbon_rect(1.0f);
                theme->draw_sprite(renderer, "failed_ribbon", L.point(Vec2{rect.x, rect.y}), L.s,
                                   Color{1.0f, 1.0f, 1.0f, alpha});
            }
        }
    }

    const std::array<float, 3>& hold_cols = hold_cols_;

    const std::array<Rect, 3> panels = {art::stat_panel_rect(0), art::stat_panel_rect(1),
                                        art::stat_panel_rect(2)};

    // 6. Digits: panel values (white), then the chrome percentage.
    if (theme != nullptr) {
        const BitmapDigits& white = theme->digits_white();
        if (stats_alpha > 0.0f) {
            const Color tint{1.0f, 1.0f, 1.0f, stats_alpha};
            const float value_top_off = art::kValueBaseline - art::kDigitBaselineRef;
            const float value_centre_off = art::kValueBaseline - art::kDigitCapHalfRef;
            const Rect& combo = panels[0];
            white.draw(renderer, max_combo_text_,
                       L.x(art::stat_text_x(combo, combo.y + value_centre_off)),
                       L.y(combo.y + value_top_off), L.s, DigitAlign::Left, tint);

            const Rect& dp = panels[1];
            const float dp_x = L.x(art::stat_text_x(dp, dp.y + value_centre_off));
            white.draw(renderer, dp_text_, dp_x, L.y(dp.y + value_top_off), L.s, DigitAlign::Left,
                       tint);
            white.draw(renderer, dp_max_text_,
                       dp_x + white.measure(dp_text_, L.s) + L.px(art::kDpGap),
                       L.y(dp.y + digit_top(art::kValueBaseline, art::kDpMaxScale)),
                       L.s * art::kDpMaxScale, DigitAlign::Left,
                       with_alpha(theme::color::kSteel, stats_alpha));

            const Rect& holds = panels[2];
            const float hold_x = art::stat_text_x(
                holds, holds.y + art::kHoldBaseline - art::kDigitCapHalfRef * art::kHoldScale);
            for (std::size_t i = 0; i < 3; ++i) {
                white.draw(renderer, hold_texts_[i], L.x(hold_x + hold_cols[i]),
                           L.y(holds.y + digit_top(art::kHoldBaseline, art::kHoldScale)),
                           L.s * art::kHoldScale, DigitAlign::Left, tint);
            }
        }
        // Counts up to (and never past) the exact C7 value; full alpha, as before.
        theme->digits_chrome().draw(
            renderer, format_percent(summary_.percent * ResultsAnimator::percent_progress(elapsed)),
            L.x(art::kPercentCentreX), L.y(art::kPercentGlyphTop), L.s, DigitAlign::Centre);
    }

    // 7. TrueType text.
    if (text != nullptr) {
        if (title_alpha > 0.0f) {
            if (!fitted_badge_.empty()) {
                const theme::TextStyle style = with_color(
                    theme::text::kBarBadge, with_alpha(badge_.colors.ink, title_alpha));
                text->draw(renderer, fitted_badge_, L.x(bar.badge_text_x),
                           L.y(bar.plate.y) + (L.px(bar.plate.h) - text->line_height(style)) * 0.5f,
                           style, TextAlign::Left);
            }
            const theme::TextStyle title_style =
                with_color(theme::text::kBarSongTitle,
                           with_alpha(theme::text::kBarSongTitle.color, title_alpha));
            text->draw(renderer, fitted_title_, L.x(bar.title_x),
                       baseline_top(*text, L, bar.baseline, title_style), title_style,
                       TextAlign::Left);
            if (!fitted_subtitle_.empty()) {
                const theme::TextStyle sub_style =
                    with_color(theme::text::kBarSongSubtitle,
                               with_alpha(theme::text::kBarSongSubtitle.color, title_alpha));
                text->draw(renderer, fitted_subtitle_, L.x(subtitle_x_),
                           baseline_top(*text, L, bar.baseline, sub_style), sub_style,
                           TextAlign::Left);
            }
            const theme::TextStyle artist_style = with_color(
                theme::text::kBarArtist, with_alpha(theme::text::kBarArtist.color, title_alpha));
            text->draw(renderer, fitted_artist_, L.x(art::kBarRight),
                       baseline_top(*text, L, bar.baseline, artist_style), artist_style,
                       TextAlign::Right);
        }

        if (stats_alpha > 0.0f) {
            const theme::TextStyle label =
                with_color(theme::text::kStatLabel, with_alpha(theme::color::kIce, stats_alpha));
            const float label_centre = art::kStatLabelTop + art::kStatLabelCapCentre;
            const std::array<std::string_view, 2> titles = {"MAX COMBO", "DANCE POINTS"};
            for (std::size_t i = 0; i < 2; ++i) {
                const Rect& panel = panels[i];
                text->draw(renderer, titles[i], L.x(art::stat_text_x(panel, panel.y + label_centre)),
                           L.y(panel.y + art::kStatLabelTop), label, TextAlign::Left);
            }
            const Rect& holds = panels[2];
            const float holds_x = art::stat_text_x(holds, holds.y + label_centre);
            const std::array<Color, 3> hold_colors = {theme::color::kHoldOkLabel,
                                                      theme::color::kMissLabel,
                                                      theme::color::kMissLabel};
            for (std::size_t i = 0; i < 3; ++i) {
                const theme::TextStyle style =
                    with_color(theme::text::kStatLabel, with_alpha(hold_colors[i], stats_alpha));
                text->draw(renderer, kHoldLabels[i], L.x(holds_x + hold_cols[i]),
                           L.y(holds.y + art::kStatLabelTop), style, TextAlign::Left);
            }
            art::draw_judgment_text(*text, renderer, L, count_texts_, stats_alpha);
        }

        if (grade_alpha > 0.0f) {
            if (!grade_sprite && !grade_fallback_text_.empty()) {
                // No grade texture: the grade text, unscaled (no mid-screen atlas bake).
                const theme::TextStyle style = with_color(
                    theme::text::kSongTitle, with_alpha(theme::color::kGold, grade_alpha));
                text->draw(renderer, grade_fallback_text_, L.x(art::kGradeCentre.x),
                           L.y(art::kGradeCentre.y) - text->line_height(style) * 0.5f, style,
                           TextAlign::Centre);
            }
            const theme::TextStyle tier = with_color(
                theme::text::kTierLabel, with_alpha(theme::text::kTierLabel.color, grade_alpha));
            text->draw(renderer, tier_text_, L.x(art::kGradeCentre.x),
                       baseline_top(*text, L, art::kTierLabelBaseline, tier), tier,
                       TextAlign::Centre);
        }

        art::draw_hint_text(*text, renderer, L, hint_word());
    }

    // 8. Overlay.
    if (theme != nullptr) {
        art::draw_scanlines(*theme, renderer, w, h, L);
    }
}

} // namespace blaze4k
