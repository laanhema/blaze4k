#include "gameplay/note_field_renderer.hpp"

#include <algorithm>

namespace td {

namespace {

Rect centered_quad(double center_x, double center_y, double width, double height) {
    return Rect{
        static_cast<float>(center_x - width * 0.5),
        static_cast<float>(center_y - height * 0.5),
        static_cast<float>(width),
        static_cast<float>(height),
    };
}

} // namespace

void NoteFieldRenderer::render(const NoteField& field,
                               const std::vector<NoteRenderItem>& items,
                               int screen_w,
                               int screen_h,
                               const NoteSkin& skin,
                               GlQuadRenderer& renderer) const {
    last_drawn_quads_ = 0;
    if (!renderer.is_initialized() || screen_h <= 0 || screen_w <= 0) {
        return;
    }

    const double field_left = (static_cast<double>(screen_w) - field.field_width()) * 0.5;
    const double receptor_y = field.screen_y(0.0);

    // 1. Receptor row.
    for (int column = 0; column < 4; ++column) {
        const NoteStyle& style = skin.style_for(NoteType::Tap);
        const Color tint = skin.column_tint(column);
        const Rect quad = centered_quad(field.column_x(column, field_left), receptor_y,
                                        style.width, style.height);
        renderer.draw_textured_quad(quad, skin.receptor_texture(), UVRect{}, tint);
        last_drawn_quads_++;
    }

    const Texture& quad_texture = skin.quad_texture();

    // 2. Hold/roll bodies.
    for (const NoteRenderItem& item : items) {
        if (!item.has_body) {
            continue;
        }
        const NoteStyle& style = skin.style_for(item.type);
        const Color tint = skin.column_tint(item.column);

        const double head_y = field.screen_y(item.head_offset);
        const double tail_y = field.screen_y(item.tail_offset);
        const double top = std::min(head_y, tail_y);
        const double bottom = std::max(head_y, tail_y);
        const double width = style.width * 0.6;

        const Rect body{
            static_cast<float>(field.column_x(item.column, field_left) - width * 0.5),
            static_cast<float>(top),
            static_cast<float>(width),
            static_cast<float>(bottom - top),
        };
        renderer.draw_textured_quad(body, quad_texture, UVRect{}, multiply(with_alpha(style.body_color, 0.65f), tint));
        last_drawn_quads_++;
    }

    // 3. Tail caps.
    for (const NoteRenderItem& item : items) {
        if (!item.has_body) {
            continue;
        }
        const NoteStyle& style = skin.style_for(item.type);
        const Color tint = skin.column_tint(item.column);
        const double cap_height = style.height * 0.3;
        const Rect cap = centered_quad(field.column_x(item.column, field_left),
                                       field.screen_y(item.tail_offset),
                                       style.width, cap_height);
        renderer.draw_textured_quad(cap, quad_texture, UVRect{}, multiply(style.tail_color, tint));
        last_drawn_quads_++;
    }

    // 4. Heads (taps, hold heads, roll heads).
    for (const NoteRenderItem& item : items) {
        if (item.type == NoteType::Mine) {
            continue;
        }
        const NoteStyle& style = skin.style_for(item.type);
        const Color tint = skin.column_tint(item.column);
        const Rect head = centered_quad(field.column_x(item.column, field_left),
                                        field.screen_y(item.head_offset),
                                        style.width, style.height);
        renderer.draw_textured_quad(head, quad_texture, UVRect{}, multiply(style.head_color, tint));
        last_drawn_quads_++;
    }

    // 5. Mines (smaller, drawn last so they sit on top).
    for (const NoteRenderItem& item : items) {
        if (item.type != NoteType::Mine) {
            continue;
        }
        const NoteStyle& style = skin.style_for(NoteType::Mine);
        const Color tint = skin.column_tint(item.column);
        const Rect mine = centered_quad(field.column_x(item.column, field_left),
                                        field.screen_y(item.head_offset),
                                        style.width, style.height);
        renderer.draw_textured_quad(mine, quad_texture, UVRect{}, multiply(style.head_color, tint));
        last_drawn_quads_++;
    }
}

} // namespace td
