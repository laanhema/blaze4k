#pragma once

#include <string>

#include "gameplay/score_keeper.hpp"
#include "render/geometry.hpp"

namespace blaze4k {

class GlQuadRenderer;

// OpenITG PercentageDisplay formatting (PercentageDisplay.cpp:135-146): add a
// +0.000001 boost, TRUNCATE (never round) to two decimals, and clamp the display
// value to [0,1]. The grade itself uses the unclamped `ScoreState::percent`.
[[nodiscard]] std::string format_percent(double percent);

// Plain integer combo text (e.g. "123"); the HUD adds the "x" suffix.
[[nodiscard]] std::string format_combo(int combo);

// Compact display label for a grade tier: star tiers render as asterisks
// (`quad_star` -> "****"), letter grades pass through unchanged ("S+", "A-", ...).
[[nodiscard]] std::string format_grade(const GradeTier& grade);

// Shared judgment palette (Blaze 4k presentation, unsourced): maps a log event's
// kind/window/hold outcome to its HUD chip color. Used by the HUD counts and the
// D2 judgment pop so both read identically.
[[nodiscard]] Color judgment_color(JudgmentKind kind, TapJudgment window, HoldJudgment hold);

// Screen rects of the top-left percent text and the bottom-centre grade text, as
// `HudRenderer::render` draws them (Blaze 4k presentation). Exposed so layout
// checks (e.g. life bar clearance) read the real HUD geometry.
[[nodiscard]] Rect percent_text_rect(const std::string& text);
[[nodiscard]] Rect grade_text_rect(const std::string& text, int screen_w, int screen_h);

// Minimum clearance between the life bar frame's right edge and the note field
// (Blaze 4k presentation, unsourced).
inline constexpr float kLifeBarFieldGap = 16.0f;

// Vertical life bar geometry (Blaze 4k presentation): anchored to the left screen
// edge, centred vertically over ~60% of the height, filling bottom (empty) to top
// (full), and clamped to stay at least a small gap left of `field_left` (the note
// field's left edge). `visible` is false for non-positive screen sizes.
struct LifeBarLayout {
    bool visible = false;
    bool danger = false;  // clamped life < 0.3
    Rect frame{};         // backing frame quad (inner rect grown by the border)
    Rect back{};          // empty-bar background
    Rect fill{};          // filled portion, bottom-anchored inside `back`
};
[[nodiscard]] LifeBarLayout layout_life_bar(double life, int screen_w, int screen_h,
                                            double field_left);

// Minimal live HUD: score percent, combo, per-window judgment counts, and grade,
// drawn as solid quads with a self-contained 5x7 bitmap font. No font asset, no
// stb_truetype, no GL context required to construct (drawing is a no-op when the
// renderer is uninitialized, which is also how `GlQuadRenderer` behaves).
class HudRenderer {
public:
    void render(const ScoreState& state, int screen_w, int screen_h,
                GlQuadRenderer& renderer) const;

    // ITG-style vertical life bar on the left side of the screen, kept clear of the
    // note field whose left edge is `field_left` (geometry: see `layout_life_bar`).
    // `life` is clamped to [0,1]; the fill takes the danger tint below the arcade
    // threshold (0.3, metrics.ini:2565). Solid quads only: no font, no stb_truetype,
    // no asset.
    void render_life(double life, int screen_w, int screen_h, double field_left,
                     GlQuadRenderer& renderer) const;
};

} // namespace blaze4k
