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

// ITG timing color for a note: tap/hold/roll art is tinted by the beat
// subdivision the note lands on, not by its column.
Color note_tint(const NoteSkin& skin, const NoteRenderItem& item) {
    const NoteQuantization quantization =
        item.note != nullptr ? item.note->quantization : NoteQuantization::Fourth;
    return skin.quantization_color(quantization);
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
        renderer.draw_textured_quad(quad, skin.receptor_texture(column), UVRect{}, tint);
        last_drawn_quads_++;
    }

    const Texture& quad_texture = skin.quad_texture();
    const Texture& body_texture = skin.body_texture();

    // 2. Hold/roll bodies.
    for (const NoteRenderItem& item : items) {
        if (!item.has_body) {
            continue;
        }
        const NoteStyle& style = skin.style_for(item.type);
        const Color tint = note_tint(skin, item);

        double head_y = field.screen_y(item.head_offset);
        const double tail_y = field.screen_y(item.tail_offset);
        // While a hit hold is being held, the head has scrolled past the
        // receptor; clamp the leading edge to the receptor so the body recedes
        // (OpenITG draws the remaining, still-held portion of the hold).
        if (item.head_hidden) {
            if (field.config().direction == ScrollDirection::Down) {
                head_y = std::min(head_y, receptor_y);
            } else {
                head_y = std::max(head_y, receptor_y);
            }
        }
        const double top = std::min(head_y, tail_y);
        const double bottom = std::max(head_y, tail_y);
        const double width = style.width * 0.6;

        const Rect body{
            static_cast<float>(field.column_x(item.column, field_left) - width * 0.5),
            static_cast<float>(top),
            static_cast<float>(width),
            static_cast<float>(bottom - top),
        };
        renderer.draw_textured_quad(body, body_texture, UVRect{}, multiply(with_alpha(style.body_color, 0.65f), tint));
        last_drawn_quads_++;
    }

    // 3. Tail caps.
    for (const NoteRenderItem& item : items) {
        if (!item.has_body) {
            continue;
        }
        const NoteStyle& style = skin.style_for(item.type);
        const Color tint = note_tint(skin, item);
        const double cap_height = style.height * 0.3;
        const Rect cap = centered_quad(field.column_x(item.column, field_left),
                                       field.screen_y(item.tail_offset),
                                       style.width, cap_height);
        renderer.draw_textured_quad(cap, quad_texture, UVRect{}, multiply(style.tail_color, tint));
        last_drawn_quads_++;
    }

    // 4. Heads (taps, hold heads, roll heads).
    for (const NoteRenderItem& item : items) {
        if (item.type == NoteType::Mine || item.head_hidden) {
            continue;
        }
        const NoteStyle& style = skin.style_for(item.type);
        const Color tint = note_tint(skin, item);
        const Rect head = centered_quad(field.column_x(item.column, field_left),
                                        field.screen_y(item.head_offset),
                                        style.width, style.height);
        renderer.draw_textured_quad(head, skin.head_texture(item.type, item.column), UVRect{},
                                    multiply(style.head_color, tint));
        last_drawn_quads_++;
    }

    // 5. Mines (smaller, drawn last so they sit on top). Mines keep their own
    // red art, so they are not quantization-tinted.
    for (const NoteRenderItem& item : items) {
        if (item.type != NoteType::Mine) {
            continue;
        }
        const NoteStyle& style = skin.style_for(NoteType::Mine);
        const Rect mine = centered_quad(field.column_x(item.column, field_left),
                                        field.screen_y(item.head_offset),
                                        style.width, style.height);
        renderer.draw_textured_quad(mine, skin.head_texture(NoteType::Mine, item.column), UVRect{},
                                    style.head_color);
        last_drawn_quads_++;
    }
}

} // namespace td
