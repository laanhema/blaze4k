#ifndef BLAZE4K_VERSION
#error "BLAZE4K_VERSION must come from CMake project(VERSION ...)"
#endif

#include "screens/title_art.hpp"

#include <cmath>

#include "gameplay/noteskin.hpp"
#include "render/gl_quad_renderer.hpp"
#include "render/theme.hpp"
#include "render/theme_layout.hpp"
#include "render/theme_textures.hpp"

namespace blaze4k::title_art {

NoteQuantization arrow_quantization(int column) {
    switch (column) {
    case 1:
        return NoteQuantization::Eighth;
    case 2:
        return NoteQuantization::Twelfth;
    case 3:
        return NoteQuantization::Sixteenth;
    default:
        return NoteQuantization::Fourth;
    }
}

Vec2 arrow_centre(const theme::LayoutScale& L, int column) {
    const double ref_x =
        theme::layout::kRefWidth * 0.5 + (static_cast<double>(column) - 1.5) * NoteSkin::kColumnWidth;
    const double ref_y = theme::layout::kTitleArrowsTop + NoteSkin::kNoteSize * 0.5;
    return Vec2{L.x(static_cast<float>(ref_x)), L.y(static_cast<float>(ref_y))};
}

float arrow_box(const theme::LayoutScale& L) {
    return L.px(static_cast<float>(NoteSkin::kNoteSize));
}

Vec2 centred_sprite_pos(const theme::LayoutScale& L, float ref_top, Vec2 content_size) {
    return Vec2{std::round(L.x(theme::layout::kRefWidth * 0.5f) - content_size.x * 0.5f),
                std::round(L.y(ref_top))};
}

bool prompt_visible(double blink_seconds) {
    return (static_cast<int>(blink_seconds * 2.0) % 2) == 0;
}

float attract_pulse(double phase_seconds) {
    return 0.65f + 0.35f * static_cast<float>(std::sin(phase_seconds * 2.0));
}

int attract_active_receptor(double phase_seconds) {
    return static_cast<int>(phase_seconds * 4.0) % 4;
}

std::string_view footer_left_text() {
    return "SINGLE \xC2\xB7 4 PANEL";
}

std::string_view footer_right_text() {
    return "BLAZE 4K v" BLAZE4K_VERSION;
}

float footer_text_top(const theme::LayoutScale& L, float line_height) {
    using theme::layout::kFooterHeight;
    using theme::layout::kRefHeight;
    return L.y(kRefHeight - kFooterHeight) + (L.px(kFooterHeight) - line_height) * 0.5f;
}

void draw_backdrop(const ThemeTextures& theme, GlQuadRenderer& renderer, int w, int h) {
    theme.draw_stretch(renderer, "bg_title",
                       Rect{0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h)});
}

void draw_centred_sprite(const ThemeTextures& theme, GlQuadRenderer& renderer,
                         std::string_view name, const theme::LayoutScale& L, float ref_top,
                         Color tint) {
    const Vec2 size = theme.content_size(name, L.s);
    theme.draw_sprite(renderer, name, centred_sprite_pos(L, ref_top, size), L.s, tint);
}

void draw_scanlines(const ThemeTextures& theme, GlQuadRenderer& renderer, int w, int h, float s) {
    theme.draw_tiled(renderer, "scanlines",
                     Rect{0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h)}, s);
}

} // namespace blaze4k::title_art
