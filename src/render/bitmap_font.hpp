#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "render/geometry.hpp"

namespace blaze4k {

class GlQuadRenderer;

// Shared 5x7 bitmap font. Coordinates are pixels with a top-left origin (y down).
// A headless (uninitialized) renderer makes every draw a no-op.

// Text is UTF-8 and may be malformed (simfile titles are untrusted). Each
// visible code point takes exactly one 6*pixel cell: its native glyph
// (printable ASCII), else its one-to-one ASCII fold, else a hollow placeholder
// box. Zero-width code points (combining marks, ZWJ, variation selectors, BOM)
// draw nothing and take no cell. Malformed bytes decode to U+FFFD (placeholder).

// True when `cp` has a native glyph (printable ASCII 0x20-0x7E).
[[nodiscard]] bool has_glyph(char32_t cp);

// The 7 row bitmaps draw_text uses for `cp` (5 bits per row, bit 4 leftmost):
// native glyph, else its ASCII fold, else the placeholder box. nullptr for
// zero-width code points (no cell).
[[nodiscard]] const std::uint8_t* glyph_rows(char32_t cp);

// True when every non-zero-width code point of UTF-8 `text` has a native glyph
// (folds and placeholders do not count). Empty text is covered.
[[nodiscard]] bool font_covers_text(std::string_view text);

// Pixel width of UTF-8 `text` at `pixel` scale (one 6*pixel cell per visible
// code point).
[[nodiscard]] float text_width(const std::string& text, float pixel);

// Draws UTF-8 `text` with its top-left at (x, y). Undrawable code points draw
// the placeholder box; zero-width code points are skipped.
void draw_text(GlQuadRenderer& renderer, const std::string& text, float x, float y, float pixel,
               Color color);

// Draws `text` horizontally centered on `center_x` with top edge at `y`.
void draw_text_centered(GlQuadRenderer& renderer, const std::string& text, float center_x, float y,
                        float pixel, Color color);

} // namespace blaze4k