#pragma once

// Cabinet v3 title/attract art (#92), shared by TitleScreen and AttractScreen.
//
//  - Pure layout helpers: reference-space (1280x720) positions mapped to window
//    pixels with theme::layout_scale (#91). GL-free, so title_screen_test pins
//    them headless.
//  - Thin draw helpers over ThemeTextures (#89) and NoteSkin sprites. Each one is
//    a no-op on an uninitialised GlQuadRenderer, and draw_skin_sprite skips an
//    invalid texture (the renderer would otherwise draw a solid white quad).
//
// Presentation only: nothing here reads the music clock; animation runs on the
// screens' fixed `dt`.

#include <string_view>

#include "chart/note.hpp"
#include "render/geometry.hpp"
#include "render/theme_layout.hpp"

namespace blaze4k {

class GlQuadRenderer;
class ThemeTextures;
struct SkinSprite;

namespace title_art {

// The title's tap notes are static: frame 0 of the 2-beat Cel cycle.
inline constexpr double kArrowBeat = 0.0;

// Fixed quantization colour per column: Left 4th (red), Down 8th (blue),
// Up 12th (purple), Right 16th (yellow). Out-of-range columns give Fourth.
[[nodiscard]] NoteQuantization arrow_quantization(int column);

// Screen centre of the title arrow / attract receptor in `column` (0..3):
// reference x = 640 + (column - 1.5) * 108, y = kTitleArrowsTop + 48.
[[nodiscard]] Vec2 arrow_centre(const theme::LayoutScale& L, int column);

// Arrow box edge in screen px (NoteSkin::kNoteSize at the layout scale).
[[nodiscard]] float arrow_box(const theme::LayoutScale& L);

// Content top-left of a sprite centred on reference x = 640 with its content top
// at reference `ref_top`; both rounded to whole pixels.
[[nodiscard]] Vec2 centred_sprite_pos(const theme::LayoutScale& L, float ref_top,
                                      Vec2 content_size);

// PRESS START blink: on for 0.5 s, off for 0.5 s (1 Hz).
[[nodiscard]] bool prompt_visible(double blink_seconds);

// Attract brightness pulse, 0.65 + 0.35 * sin(2t), always in [0.30, 1.00].
[[nodiscard]] float attract_pulse(double phase_seconds);

// Attract receptor lit at `phase_seconds` (4 Hz sequence 0, 1, 2, 3).
[[nodiscard]] int attract_active_receptor(double phase_seconds);

// Footer strings: "SINGLE · 4 PANEL" (UTF-8) and "BLAZE 4K v<BLAZE4K_VERSION>".
[[nodiscard]] std::string_view footer_left_text();
[[nodiscard]] std::string_view footer_right_text();

// Footer text top: the line box centred in the bottom kFooterHeight band.
[[nodiscard]] float footer_text_top(const theme::LayoutScale& L, float line_height);

// True when the sprite has a texture that was actually uploaded.
[[nodiscard]] bool skin_sprite_drawable(const SkinSprite& sprite);

// bg_title stretched over the whole window {0, 0, w, h} (manifest: "stretched to
// the window").
void draw_backdrop(const ThemeTextures& theme, GlQuadRenderer& renderer, int w, int h);

// The `name` sprite centred on reference x = 640 with its content top at `ref_top`.
void draw_centred_sprite(const ThemeTextures& theme, GlQuadRenderer& renderer,
                         std::string_view name, const theme::LayoutScale& L, float ref_top,
                         Color tint = Color{});

// `sprite` in a square of `box * sprite.scale` centred on `centre`; nothing when
// the sprite is not drawable.
void draw_skin_sprite(GlQuadRenderer& renderer, const SkinSprite& sprite, Vec2 centre, float box);

// The scanlines tile over the whole window (one texel per screen pixel).
void draw_scanlines(const ThemeTextures& theme, GlQuadRenderer& renderer, int w, int h, float s);

} // namespace title_art
} // namespace blaze4k
