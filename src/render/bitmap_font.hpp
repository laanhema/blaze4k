#pragma once

#include <string>

#include "render/geometry.hpp"

namespace blaze4k {

class GlQuadRenderer;

// Shared 5x7 bitmap font. Coordinates are pixels with a top-left origin (y down).
// A headless (uninitialized) renderer makes every draw a no-op.

// Pixel width of `text` at `pixel` scale (each glyph cell is 6*pixel wide).
[[nodiscard]] float text_width(const std::string& text, float pixel);

// Draws `text` with its top-left at (x, y). Unknown glyphs advance the cursor
// without drawing.
void draw_text(GlQuadRenderer& renderer, const std::string& text, float x, float y, float pixel,
               Color color);

// Draws `text` horizontally centered on `center_x` with top edge at `y`.
void draw_text_centered(GlQuadRenderer& renderer, const std::string& text, float center_x, float y,
                        float pixel, Color color);

} // namespace blaze4k