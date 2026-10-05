#pragma once

#include <string>

#include "render/geometry.hpp"
#include "render/theme.hpp"

namespace blaze4k {

class GlQuadRenderer;
class TextRenderer;
class ThemeTextures;

// OpenITG PercentageDisplay formatting (PercentageDisplay.cpp:135-146): add a
// +0.000001 boost, TRUNCATE (never round) to two decimals, and clamp the display
// value to [0,1]. The grade itself uses the unclamped `ScoreState::percent`.
// No longer drawn during gameplay (#93); the results and select screens use it.
[[nodiscard]] std::string format_percent(double percent);

// Plain integer combo text (e.g. "123"), drawn by the combo line under the judgment.
[[nodiscard]] std::string format_combo(int combo);

// Minimum clearance between the life bar frame's right edge (and the difficulty
// badge) and the note field (Blaze 4k presentation, unsourced). Screen px.
inline constexpr float kLifeBarFieldGap = 16.0f;
// life_frame chrome border in reference px (the manifest's 8 px @2x slice9 border).
inline constexpr float kLifeFrameBorderRef = 4.0f;
// Narrowest life track in very narrow windows, screen px.
inline constexpr float kLifeBarMinTrack = 6.0f;
// Difficulty badge text inset from the plate's left edge, reference px (mock: "HARD 8"
// ink starts 18 px into the plate).
inline constexpr float kBadgeTextPadX = 18.0f;
// The badge plate grows with long Edit names up to this width, reference px; longer
// text is truncated with "...".
inline constexpr float kBadgeMaxWidthRef = 300.0f;

// Cabinet vertical life bar geometry (#93): the life_frame starts at
// theme::layout::kLifeBar (layout-scaled), slides left and then shrinks to stay at
// least kLifeBarFieldGap left of `field_left` (the note field's left edge). The track
// is the frame inset by the chrome border; the fill is bottom-anchored inside it.
// `visible` is false for non-positive screen sizes.
struct LifeBarLayout {
    bool visible = false;
    bool danger = false;    // fraction < theme::color::kLifeDangerThreshold
    float fraction = 0.0f;  // clamped life in [0,1] (NaN = 0)
    float border = 0.0f;    // chrome border, screen px
    Rect frame{};           // life_frame (slice9) content rect
    Rect track{};           // fill area inside the frame
    Rect fill{};            // filled portion, bottom-anchored inside `track`
};
[[nodiscard]] LifeBarLayout layout_life_bar(double life, int screen_w, int screen_h,
                                            double field_left);

// The gameplay difficulty badge content: e.g. "HARD 8" with theme::difficulty::kHard.
// Built once per song by the screens layer (difficulty_badge_for).
struct DifficultyBadge {
    std::string text;
    theme::DifficultyColors colors = theme::difficulty::kEdit;
};

// Difficulty badge geometry: the diff_badge plate at theme::layout::kDiffBadge, widened
// for text of width `text_w` (screen px) up to kBadgeMaxWidthRef, and narrowed to stay
// clear of the note field. Hidden when it would be narrower than kDiffBadge.w or for a
// non-positive screen size. The text pen starts kBadgeTextPadX into the plate.
struct DiffBadgeLayout {
    bool visible = false;
    Rect plate{};
    float text_x = 0.0f;
    float text_max_w = 0.0f;
};
[[nodiscard]] DiffBadgeLayout layout_diff_badge(float text_w, int screen_w, int screen_h,
                                                double field_left);

// Cabinet gameplay HUD (#93): the difficulty badge and the chrome life bar, drawn over
// ThemeTextures / TextRenderer. The live percent and the per-window judgment chips were
// removed in #93; the score keeper still counts everything for results.
// Every draw is a no-op for a null service or an uninitialised renderer.
class HudRenderer {
public:
    // Badge plate (tinted by the difficulty fill), then the life frame, fill (or the
    // danger fill below the arcade threshold) and the bottom-aligned stripes. `text`
    // only measures the badge text; null gives the minimum plate. No-op for a null theme.
    void render_chrome(const DifficultyBadge& badge, double life, int screen_w, int screen_h,
                       double field_left, const ThemeTextures* theme, const TextRenderer* text,
                       GlQuadRenderer& renderer) const;

    // Badge text in the difficulty ink colour, truncated to the plate. The truncated
    // label is cached and recomputed only when the text, text scale or width changes.
    void render_text(const DifficultyBadge& badge, int screen_w, int screen_h, double field_left,
                     TextRenderer* text, GlQuadRenderer& renderer);

private:
    std::string cached_text_;
    std::string cached_source_;
    float cached_max_w_ = -1.0f;
    float cached_scale_ = -1.0f;
};

} // namespace blaze4k
