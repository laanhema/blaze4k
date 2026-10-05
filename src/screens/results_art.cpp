#include "screens/results_art.hpp"

#include <algorithm>
#include <cmath>

#include "render/gl_quad_renderer.hpp"
#include "render/texture.hpp"
#include "render/theme_textures.hpp"
#include "render/ttf_font.hpp"
#include "screens/results.hpp"
#include "screens/select_art.hpp"
#include "screens/song_display_text.hpp"
#include "screens/title_art.hpp"

namespace blaze4k::results_art {

namespace layout = theme::layout;

namespace {

// Non-finite -> 0, so no layout output is ever NaN.
float finite_or_zero(float v) {
    return std::isfinite(v) ? v : 0.0f;
}

// Line-box top that centres `style`'s line in a reference band [ref_top, ref_top + ref_h].
float centred_top(const TextRenderer& text, const theme::LayoutScale& L, float ref_top, float ref_h,
                  const theme::TextStyle& style) {
    return L.y(ref_top) + (L.px(ref_h) - text.line_height(style)) * 0.5f;
}

// A solid quad from reference-space corners.
void draw_solid(GlQuadRenderer& renderer, const theme::LayoutScale& L,
                const std::array<Vec2, 4>& ref_corners, Color color) {
    static const Texture kSolid; // id 0: the renderer substitutes its white texture
    const std::array<Vec2, 4> corners = {L.point(ref_corners[0]), L.point(ref_corners[1]),
                                         L.point(ref_corners[2]), L.point(ref_corners[3])};
    renderer.draw_quad_points(corners, kSolid, UVRect{}, {color, color, color, color});
}

constexpr std::array<Color, kJudgmentRowCount> kRowColors = {
    theme::color::kFantastic, theme::color::kExcellent, theme::color::kGreat,
    theme::color::kDecent,    theme::color::kWayOff,    theme::color::kMiss,
};

std::size_t row_index(int i) {
    return static_cast<std::size_t>(std::clamp(i, 0, static_cast<int>(kJudgmentRowCount) - 1));
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Pure layout
// ---------------------------------------------------------------------------------------------

TopBarLayout top_bar_layout(float badge_text_w, float title_w, float artist_w, float subtitle_w) {
    badge_text_w = finite_or_zero(badge_text_w);
    title_w = finite_or_zero(title_w);
    artist_w = finite_or_zero(artist_w);
    subtitle_w = finite_or_zero(subtitle_w);
    // A subtitle needs a title to follow ("UNKNOWN" stands in when there is no song).
    const bool has_subtitle = subtitle_w > 0.0f && title_w > 0.0f;

    TopBarLayout out;
    const bool has_artist = artist_w > 0.0f;
    out.artist_max_w = has_artist ? std::min(artist_w, kBarArtistMax) : 0.0f;
    out.artist_x = kBarRight - out.artist_max_w;
    const float artist_gap = has_artist ? kBarGap : 0.0f;

    const bool has_plate = badge_text_w > 0.0f;
    out.badge_text_max_w = has_plate ? std::min(badge_text_w, kBarBadgeTextMax) : 0.0f;
    const float plate_w = has_plate ? out.badge_text_max_w + 2.0f * kBarBadgePadX : 0.0f;
    const float plate_gap = has_plate ? kBarGap : 0.0f;

    const float title_right = out.artist_x - artist_gap;
    const float title_max = std::max(0.0f, title_right - kBarLeftLimit - plate_w - plate_gap);
    // The title slot: the title alone, or title + gap + subtitle when there is one.
    const float slot_need = has_subtitle ? title_w + kBarSubtitleGap + subtitle_w : title_w;
    const float slot_w = slot_need > 0.0f ? std::min(slot_need, title_max) : 0.0f;
    out.title_x = title_right - slot_w;
    if (has_subtitle) {
        const TitleSubtitleFit fit = fit_title_subtitle(title_w, subtitle_w, kBarSubtitleGap, slot_w);
        out.title_max_w = fit.title_max_w;
        out.subtitle_max_w = fit.subtitle_max_w;
        out.subtitle_x =
            fit.subtitle_max_w > 0.0f ? out.title_x + fit.title_max_w + kBarSubtitleGap : 0.0f;
    } else {
        out.title_max_w = slot_w;
    }

    const float title_gap = slot_w > 0.0f ? kBarGap : 0.0f;
    out.plate = Rect{out.title_x - (has_plate ? title_gap : 0.0f) - plate_w, kBarBadgeTop, plate_w,
                     kBarBadgeHeight};
    out.badge_text_x = out.plate.x + kBarBadgePadX;
    out.baseline = kBarTextBaseline;
    return out;
}

Rect stat_panel_rect(int i) {
    const int n = std::clamp(i, 0, static_cast<int>(kStatPanelHeights.size()) - 1);
    float y = layout::kStatPanelTop;
    for (int k = 0; k < n; ++k) {
        y += kStatPanelHeights[static_cast<std::size_t>(k)] + layout::kStatPanelGap;
    }
    return Rect{layout::kStatPanelX, y, layout::kStatPanelWidth,
                kStatPanelHeights[static_cast<std::size_t>(n)]};
}

float stat_text_x(const Rect& panel, float centre_y) {
    const float x = panel.x + kStatTextPadX +
                    theme::skew::kStatPanel * (panel.y + panel.h * 0.5f - finite_or_zero(centre_y));
    return finite_or_zero(x);
}

std::array<float, 3> hold_columns(std::array<float, 3> label_w, std::array<float, 3> value_w) {
    std::array<float, 3> out{0.0f, 0.0f, 0.0f};
    for (std::size_t i = 0; i + 1 < out.size(); ++i) {
        const float widest =
            std::max(std::max(finite_or_zero(label_w[i]), finite_or_zero(value_w[i])), 0.0f);
        out[i + 1] = out[i] + widest + kHoldColumnGap;
    }
    return out;
}

Rect grade_rect(Vec2 content, float scale) {
    scale = std::max(finite_or_zero(scale), 0.0f);
    const float w = std::max(finite_or_zero(content.x), 0.0f) * scale;
    const float h = std::max(finite_or_zero(content.y), 0.0f) * scale;
    return Rect{kGradeCentre.x - w * 0.5f, kGradeCentre.y - h * 0.5f, w, h};
}

Rect ribbon_rect(float scale) {
    scale = std::max(finite_or_zero(scale), 0.0f);
    const Rect& r = layout::kRecordRibbon;
    const float cx = r.x + r.w * 0.5f;
    const float cy = r.y + r.h * 0.5f;
    const float w = r.w * scale;
    const float h = r.h * scale;
    return Rect{cx - w * 0.5f, cy - h * 0.5f, w, h};
}

JudgmentRowLayout judgment_row_layout(int i) {
    const float top = layout::kJudgmentBarsTop +
                      static_cast<float>(row_index(i)) * layout::kJudgmentBarRowPitch;
    JudgmentRowLayout out;
    out.label_x = layout::kJudgmentBarsX;
    out.baseline = top + kJudgmentBaselineOffset;
    out.bar = Rect{layout::kJudgmentBarsX + layout::kJudgmentBarLabelWidth + kBarGapX,
                   top + kBarRowTopOffset, kJudgmentBarWidth, layout::kJudgmentBarHeight};
    out.count_right = kJudgmentCountRight;
    return out;
}

float judgment_bar_fill_width(int count, int total, float bar_w) {
    bar_w = std::max(finite_or_zero(bar_w), 0.0f);
    if (count <= 0 || total <= 0 || bar_w <= 0.0f) {
        return 0.0f;
    }
    const double w = static_cast<double>(bar_w) * static_cast<double>(count) /
                     static_cast<double>(total);
    return std::clamp(static_cast<float>(w), std::min(kMinBarFill, bar_w), bar_w);
}

int judged_tap_total(const ResultsSummary& summary) {
    long long total = 0;
    for (std::size_t i = 0; i < kJudgmentRowCount; ++i) {
        total += std::max(summary.tap_counts[i], 0);
    }
    return static_cast<int>(std::min<long long>(total, 2147483647LL));
}

std::string grade_texture_name(std::string_view label) {
    if (label.empty()) {
        return {};
    }
    std::string out = "grade_";
    for (const char c : label) {
        if (c == '+') {
            out += "_plus";
        } else if (c == '-') {
            out += "_minus";
        } else {
            out += c;
        }
    }
    return out;
}

std::string grade_tier_text(std::string_view label) {
    if (label.empty()) {
        return {};
    }
    if (label == "quad_star") {
        return "FOUR STARS";
    }
    if (label == "triple_star") {
        return "THREE STARS";
    }
    if (label == "double_star") {
        return "TWO STARS";
    }
    if (label == "single_star") {
        return "ONE STAR";
    }
    return "GRADE " + std::string(label);
}

std::string digits_text(int v) {
    return std::to_string(std::max(v, 0));
}

Color judgment_row_color(int i) {
    return kRowColors[row_index(i)];
}

Color judgment_label_color(int i) {
    return row_index(i) == kJudgmentRowCount - 1 ? theme::color::kMissLabel : judgment_row_color(i);
}

// ---------------------------------------------------------------------------------------------
// Draw helpers
// ---------------------------------------------------------------------------------------------

void draw_backdrop(const ThemeTextures& theme, GlQuadRenderer& renderer, int w, int h) {
    if (!renderer.is_initialized()) {
        return;
    }
    theme.draw_stretch(renderer, "bg_results",
                       Rect{0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h)});
}

void draw_bars(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
               const theme::LayoutScale& L, int w) {
    if (!renderer.is_initialized()) {
        return;
    }
    const bool has_sprite = theme != nullptr && theme->entry("title_score_screen") != nullptr;
    if (theme != nullptr) {
        theme->draw_stretch_x(renderer, "bar_top", 0.0f, L.y(0.0f), static_cast<float>(w), L.s);
        if (has_sprite) {
            theme->draw_sprite(renderer, "title_score_screen", L.point(kTitleSpritePos), L.s);
        }
        theme->draw_stretch(renderer, "bar_hint",
                            Rect{0.0f, L.y(kHintBarTop), static_cast<float>(w), L.px(kHintBarHeight)});
    }
    if (!has_sprite && text != nullptr) {
        const theme::TextStyle& style = theme::text::kBarSongTitle;
        text->draw(renderer, kScreenTitleText, L.x(layout::kTopBarPadX),
                   L.y(kBarTextBaseline) - text->ascent(style), style, TextAlign::Left);
    }
}

void draw_hint_text(TextRenderer& text, GlQuadRenderer& renderer, const theme::LayoutScale& L,
                    std::string_view word) {
    if (!renderer.is_initialized()) {
        return;
    }
    constexpr std::string_view kKey = "ENTER";
    const theme::TextStyle& key_style = theme::text::kHintKey;
    const theme::TextStyle& word_style = theme::text::kHintWord;
    const float key_w = ref_measure(text, kKey, key_style);
    const float word_w = ref_measure(text, word, word_style);
    const float left = kHintCentreX - (key_w + kHintKeyGap + word_w) * 0.5f;
    text.draw(renderer, kKey, L.x(left), centred_top(text, L, kHintBandTop, kHintBandHeight, key_style),
              key_style, TextAlign::Left);
    text.draw(renderer, word, L.x(left + key_w + kHintKeyGap),
              centred_top(text, L, kHintBandTop, kHintBandHeight, word_style), word_style,
              TextAlign::Left);
}

void draw_stat_panel_plates(const ThemeTextures& theme, GlQuadRenderer& renderer,
                            const theme::LayoutScale& L, float alpha) {
    if (!renderer.is_initialized() || alpha <= 0.0f) {
        return;
    }
    for (int i = 0; i < 3; ++i) {
        theme.draw_slice3(renderer, "stat_panel", L.rect(stat_panel_rect(i)),
                          Color{1.0f, 1.0f, 1.0f, alpha});
    }
}

void draw_skewed_solid(GlQuadRenderer& renderer, const theme::LayoutScale& L, const Rect& rect,
                       float skew, Color color) {
    if (!renderer.is_initialized() || rect.w <= 0.0f || rect.h <= 0.0f) {
        return;
    }
    draw_solid(renderer, L, select_art::skewed_quad(rect, skew), color);
}

void draw_judgment_bars(GlQuadRenderer& renderer, const theme::LayoutScale& L,
                        const ResultsSummary& summary, float alpha) {
    if (!renderer.is_initialized() || alpha <= 0.0f) {
        return;
    }
    const int total = judged_tap_total(summary);
    for (int i = 0; i < static_cast<int>(kJudgmentRowCount); ++i) {
        const JudgmentRowLayout row = judgment_row_layout(i);
        draw_skewed_solid(renderer, L, row.bar, kJudgmentBarSkew,
                          with_alpha(theme::color::kBarTrack, alpha));
        const float fill_w = judgment_bar_fill_width(
            summary.tap_counts[static_cast<std::size_t>(i)], total, row.bar.w);
        draw_skewed_solid(renderer, L, Rect{row.bar.x, row.bar.y, fill_w, row.bar.h},
                          kJudgmentBarSkew, with_alpha(judgment_row_color(i), alpha));
    }
}

void draw_judgment_text(TextRenderer& text, GlQuadRenderer& renderer, const theme::LayoutScale& L,
                        const std::array<std::string, kJudgmentRowCount>& counts, float alpha) {
    if (!renderer.is_initialized() || alpha <= 0.0f) {
        return;
    }
    for (int i = 0; i < static_cast<int>(kJudgmentRowCount); ++i) {
        const JudgmentRowLayout row = judgment_row_layout(i);
        const theme::TextStyle style =
            with_color(theme::text::kJudgmentLabel, with_alpha(judgment_label_color(i), alpha));
        text.draw(renderer, kJudgmentRowLabels[static_cast<std::size_t>(i)], L.x(row.label_x),
                  L.y(row.baseline) - text.ascent(style), style, TextAlign::Left);
    }
    const theme::TextStyle count_style =
        with_color(theme::text::kJudgmentCount, with_alpha(theme::color::kText, alpha));
    for (int i = 0; i < static_cast<int>(kJudgmentRowCount); ++i) {
        const JudgmentRowLayout row = judgment_row_layout(i);
        text.draw(renderer, counts[static_cast<std::size_t>(i)], L.x(row.count_right),
                  L.y(row.baseline) - text.ascent(count_style), count_style, TextAlign::Right);
    }
}

void draw_scanlines(const ThemeTextures& theme, GlQuadRenderer& renderer, int w, int h,
                    const theme::LayoutScale& L) {
    if (!renderer.is_initialized()) {
        return;
    }
    title_art::draw_scanlines(theme, renderer, w, h, L.s);
}

} // namespace blaze4k::results_art
