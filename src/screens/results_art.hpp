#pragma once

// Cabinet v3 score screen art (#95), used by ResultsScreen.

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include "render/geometry.hpp"
#include "render/theme.hpp"
#include "render/theme_layout.hpp"

namespace blaze4k {

class GlQuadRenderer;
class TextRenderer;
class ThemeTextures;
struct ResultsSummary;

namespace results_art {

inline constexpr std::string_view kScreenTitleText = "SCORE SCREEN";
inline constexpr Vec2 kTitleSpritePos{theme::layout::kTopBarPadX, 9.0f};

// Top bar group (right to left: artist, title, badge plate). Measured from
// cabinet-v3-results.png (#95) unless noted.
inline constexpr float kBarRight = theme::layout::kRefWidth - theme::layout::kTopBarPadX; // 1240
inline constexpr float kBarGap = 16.0f;          // plate -> title and title -> artist
inline constexpr float kBarSubtitleGap = 8.0f;   // title -> subtitle (#110)
inline constexpr float kBarLeftLimit = 360.0f;   // clear of the 40..340 title sprite
inline constexpr float kBarArtistMax = 240.0f;   // Blaze 4k cap (long artists truncate)
inline constexpr float kBarBadgeTextMax = 200.0f; // Blaze 4k cap (long Edit names truncate)
inline constexpr float kBarBadgePadX = 12.0f;    // plate x padding each side
inline constexpr float kBarBadgeTop = 16.0f;     // plate y 16..47
inline constexpr float kBarBadgeHeight = 32.0f;
inline constexpr float kBarTextBaseline = 40.0f; // shared title / artist baseline

// Hint bar: bar_hint stretched over the 2px rule + the 44px score-screen band
// (theme.hpp kHintBarHeight note: "score screen: 44"), measured rule y 674..675.
inline constexpr float kHintBarTop = 674.0f;
inline constexpr float kHintBarHeight = 46.0f;
inline constexpr float kHintBandTop = 676.0f;
inline constexpr float kHintBandHeight = 44.0f;
inline constexpr float kHintKeyGap = 8.0f; // after "ENTER" (select_art::kHintTextKeyGap)
inline constexpr float kHintCentreX = theme::layout::kRefWidth * 0.5f;

// Stat panels: x / top / width / gap from theme.hpp, heights measured (#95).
inline constexpr std::array<float, 3> kStatPanelHeights = {111.0f, 111.0f, 102.0f};
inline constexpr float kStatTextPadX = 30.0f;        // manifest "label at x+20" + 10 margin (#127)
inline constexpr float kStatLabelTop = 14.0f;        // manifest: label line top
inline constexpr float kStatLabelCapCentre = 14.5f;  // cap centre below the label line top (mock)
inline constexpr float kValueBaseline = 86.0f;       // MAX COMBO / DP value baseline (mock)
inline constexpr float kHoldBaseline = 79.0f;        // holds value baseline (mock)
// digits_white glyph rect: baseline 57.5 ref below the glyph top, cap half-height 16.5
// (manifest atlas ink rows 49..115 @2x).
inline constexpr float kDigitBaselineRef = 57.5f;
inline constexpr float kDigitCapHalfRef = 16.5f;
inline constexpr float kDpMaxScale = 28.0f / 48.0f;  // "/ 2000" cap 19 vs 33 (mock)
inline constexpr float kHoldScale = 40.0f / 48.0f;   // holds cap 28 vs 33 (mock)
inline constexpr float kDpGap = 8.0f;                // numerator -> "/ max" (mock)
inline constexpr float kHoldColumnMaxGap = 34.0f;    // after max(label, value) (mock)
inline constexpr float kHoldColumnMinGap = 8.0f;
inline constexpr float kHoldRowWidth = theme::layout::kStatPanelWidth - 2.0f * kStatTextPadX; // 300

// Centre column.
inline constexpr Vec2 kGradeCentre{640.0f, 286.0f};        // grade ink centre (mock)
inline constexpr Vec2 kGradeContentRef{340.0f, 200.0f};    // manifest grade_* content 680x400 @2x
inline constexpr float kTierLabelBaseline = 395.0f;        // "ONE STAR" caps 382..394 (mock)
inline constexpr float kPercentGlyphTop = 481.0f;          // chrome face top 512 - 31 (mock)
inline constexpr float kPercentCentreX = 640.0f;

// Judgment rows (x / top / pitch / height / label / count widths from theme.hpp).
inline constexpr float kBarGapX = 10.0f;               // label -> bar and bar -> count (mock)
inline constexpr float kBarRowTopOffset = 11.0f;       // bar y 131..144 in row 0 (mock)
inline constexpr float kJudgmentBaselineOffset = 25.0f; // label caps 130..144 in row 0 (mock)
inline constexpr float kMinBarFill = 2.0f;             // a single Decent still shows (mock)
// The mock's bars are straight (owner decision on #95); one constant to slant them.
inline constexpr float kJudgmentBarSkew = 0.0f;
inline constexpr float kJudgmentBarWidth =
    theme::layout::kJudgmentBarsWidth - theme::layout::kJudgmentBarLabelWidth -
    theme::layout::kJudgmentBarCountWidth - 2.0f * kBarGapX; // 154 (mock 1018..1171)
inline constexpr float kJudgmentCountRight =
    theme::layout::kJudgmentBarsX + theme::layout::kJudgmentBarsWidth; // 1238
inline constexpr std::size_t kJudgmentRowCount = 6;
inline constexpr std::array<std::string_view, kJudgmentRowCount> kJudgmentRowLabels = {
    "FANTASTIC", "EXCELLENT", "GREAT", "DECENT", "WAY OFF", "MISS"};

inline constexpr float kEmptyMessageTop = 340.0f;

static_assert(theme::layout::kJudgmentBarsX + theme::layout::kJudgmentBarLabelWidth +
                      2.0f * kBarGapX + 154.0f + theme::layout::kJudgmentBarCountWidth ==
                  theme::layout::kJudgmentBarsX + theme::layout::kJudgmentBarsWidth,
              "judgment row: label + gap + 154px bar + gap + count = kJudgmentBarsWidth");
static_assert(theme::layout::kStatPanelTop + kStatPanelHeights[0] + theme::layout::kStatPanelGap ==
                  245.0f,
              "the second stat panel starts at the mock's y 245");

struct TopBarLayout {
    Rect plate{};
    float badge_text_x = 0.0f;
    float badge_text_max_w = 0.0f;
    float title_x = 0.0f;
    float title_max_w = 0.0f;
    float artist_x = 0.0f;
    float artist_max_w = 0.0f;
    float subtitle_x = 0.0f;
    float subtitle_max_w = 0.0f;
    float baseline = kBarTextBaseline;
};
[[nodiscard]] TopBarLayout top_bar_layout(float badge_text_w, float title_w, float artist_w,
                                          float subtitle_w = 0.0f);

[[nodiscard]] Rect stat_panel_rect(int i);
[[nodiscard]] float stat_text_x(const Rect& panel, float centre_y);
[[nodiscard]] std::array<float, 3> hold_columns(std::array<float, 3> label_w,
                                                std::array<float, 3> value_w);

[[nodiscard]] Rect grade_rect(Vec2 content, float scale);
[[nodiscard]] Rect ribbon_rect(float scale);

struct JudgmentRowLayout {
    float label_x = 0.0f;
    float baseline = 0.0f;
    Rect bar{};
    float count_right = 0.0f;
};
[[nodiscard]] JudgmentRowLayout judgment_row_layout(int i);
[[nodiscard]] float judgment_bar_fill_width(int count, int total, float bar_w);
[[nodiscard]] int judged_tap_total(const ResultsSummary& summary);
[[nodiscard]] std::string grade_texture_name(std::string_view label);
[[nodiscard]] std::string grade_tier_text(std::string_view label);

[[nodiscard]] std::string digits_text(int v);

[[nodiscard]] Color judgment_row_color(int i);
[[nodiscard]] Color judgment_label_color(int i);

void draw_backdrop(const ThemeTextures& theme, GlQuadRenderer& renderer, int w, int h);
void draw_bars(const ThemeTextures* theme, TextRenderer* text, GlQuadRenderer& renderer,
               const theme::LayoutScale& L, int w);
void draw_hint_text(TextRenderer& text, GlQuadRenderer& renderer, const theme::LayoutScale& L,
                    std::string_view word);
void draw_stat_panel_plates(const ThemeTextures& theme, GlQuadRenderer& renderer,
                            const theme::LayoutScale& L, float alpha);
void draw_judgment_bars(GlQuadRenderer& renderer, const theme::LayoutScale& L,
                        const ResultsSummary& summary, float alpha);
void draw_judgment_text(TextRenderer& text, GlQuadRenderer& renderer, const theme::LayoutScale& L,
                        const std::array<std::string, kJudgmentRowCount>& counts, float alpha);
void draw_skewed_solid(GlQuadRenderer& renderer, const theme::LayoutScale& L, const Rect& rect,
                       float skew, Color color);
void draw_scanlines(const ThemeTextures& theme, GlQuadRenderer& renderer, int w, int h,
                    const theme::LayoutScale& L);

} // namespace results_art
} // namespace blaze4k
