#pragma once

#include <string>

#include "gameplay/score_keeper.hpp"

namespace td {

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

// Minimal live HUD: score percent, combo, per-window judgment counts, and grade,
// drawn as solid quads with a self-contained 5x7 bitmap font. No font asset, no
// stb_truetype, no GL context required to construct (drawing is a no-op when the
// renderer is uninitialized, which is also how `GlQuadRenderer` behaves).
class HudRenderer {
public:
    void render(const ScoreState& state, int screen_w, int screen_h,
                GlQuadRenderer& renderer) const;

    // ITG-style horizontal life bar. `life` is clamped to [0,1]; the fill takes the
    // danger tint below the arcade threshold (0.3, metrics.ini:2565). Solid quads
    // only: no font, no stb_truetype, no asset.
    void render_life(double life, int screen_w, int screen_h, GlQuadRenderer& renderer) const;
};

} // namespace td
