#pragma once

// Cabinet v3 title/attract art (#92), shared by TitleScreen and AttractScreen.

#include <string_view>

#include "chart/note.hpp"
#include "render/geometry.hpp"
#include "render/theme_layout.hpp"

namespace blaze4k {

class GlQuadRenderer;
class ThemeTextures;

namespace title_art {

inline constexpr double kArrowBeat = 0.0;

[[nodiscard]] NoteQuantization arrow_quantization(int column);
[[nodiscard]] Vec2 arrow_centre(const theme::LayoutScale& L, int column);
[[nodiscard]] float arrow_box(const theme::LayoutScale& L);
[[nodiscard]] Vec2 centred_sprite_pos(const theme::LayoutScale& L, float ref_top,
                                      Vec2 content_size);
[[nodiscard]] bool prompt_visible(double blink_seconds);
[[nodiscard]] float attract_pulse(double phase_seconds);
[[nodiscard]] int attract_active_receptor(double phase_seconds);
[[nodiscard]] std::string_view footer_left_text();
[[nodiscard]] std::string_view footer_right_text();
[[nodiscard]] float footer_text_top(const theme::LayoutScale& L, float line_height);

void draw_backdrop(const ThemeTextures& theme, GlQuadRenderer& renderer, int w, int h);
void draw_centred_sprite(const ThemeTextures& theme, GlQuadRenderer& renderer,
                         std::string_view name, const theme::LayoutScale& L, float ref_top,
                         Color tint = Color{});
void draw_scanlines(const ThemeTextures& theme, GlQuadRenderer& renderer, int w, int h, float s);

} // namespace title_art
} // namespace blaze4k
