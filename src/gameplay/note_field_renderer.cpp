#include "gameplay/note_field_renderer.hpp"

#include <algorithm>
#include <cmath>

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

NoteQuantization quantization_of(const NoteRenderItem& item) {
    return item.note != nullptr ? item.note->quantization : NoteQuantization::Fourth;
}

// Draws `sprite` in a square of `box * sprite.scale` centered on (x, y).
int draw_sprite(GlQuadRenderer& renderer, const SkinSprite& sprite, double x, double y,
                double box) {
    if (sprite.texture == nullptr) {
        return 0;
    }
    const double size = box * sprite.scale;
    renderer.set_blend_mode(sprite.blend);
    renderer.draw_textured_quad(centered_quad(x, y, size, size), *sprite.texture, sprite.uv,
                                sprite.tint, sprite.rotation);
    renderer.set_blend_mode(BlendMode::Alpha);
    return 1;
}

// Draws a hold/roll body from `head_y` to `tail_y`, then its end cap just past
// the tail. The art is authored head-on-top; when the tail is above the head
// (reverse) it is flipped vertically. A tiled body repeats from the tail toward
// the head, so the pattern moves with the note even while the head is pinned.
// Segments entirely outside [0, screen_h] are skipped. Returns quads drawn.
int draw_hold(GlQuadRenderer& renderer, const HoldSprites& hold, double x, double head_y,
              double tail_y, double screen_h) {
    if (hold.body == nullptr) {
        return 0;
    }
    int drawn = 0;
    const double width = NoteSkin::kNoteSize * hold.width_scale;
    const double length = std::abs(tail_y - head_y);
    const bool reverse = tail_y < head_y;
    const double toward_head = reverse ? 1.0 : -1.0; // screen-y step from tail to head
    const double tile = hold.tile_scale > 0.0f ? NoteSkin::kNoteSize * hold.tile_scale : length;

    // Segment [near, far] in distance from the tail; texture v runs 1 at a tile's
    // tail edge to 0 at its head edge.
    for (double near = 0.0; tile > 0.0 && near < length; near += tile) {
        const double far = std::min(near + tile, length);
        const double y_near = tail_y + toward_head * near;
        const double y_far = tail_y + toward_head * far;
        const double top = std::min(y_near, y_far);
        const double bottom = std::max(y_near, y_far);
        if (bottom < 0.0 || top > screen_h) {
            continue;
        }
        const float v_near = 1.0f;
        const float v_far = static_cast<float>(1.0 - (far - near) / tile);
        const UVRect uv = reverse ? UVRect{0.0f, v_near, 1.0f, v_far}
                                  : UVRect{0.0f, v_far, 1.0f, v_near};
        const Rect quad{static_cast<float>(x - width * 0.5), static_cast<float>(top),
                        static_cast<float>(width), static_cast<float>(bottom - top)};
        renderer.draw_textured_quad(quad, *hold.body, uv, hold.tint);
        ++drawn;
    }

    if (hold.cap != nullptr) {
        const double cap_top = reverse ? tail_y - width : tail_y;
        const UVRect uv = reverse ? UVRect{0.0f, 1.0f, 1.0f, 0.0f} : UVRect{};
        const Rect quad{static_cast<float>(x - width * 0.5), static_cast<float>(cap_top),
                        static_cast<float>(width), static_cast<float>(width)};
        renderer.draw_textured_quad(quad, *hold.cap, uv, hold.tint);
        ++drawn;
    }
    return drawn;
}

} // namespace

void NoteFieldRenderer::render(const NoteField& field,
                               const std::vector<NoteRenderItem>& items,
                               const NoteFieldFrame& frame,
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
    const double note_size = NoteSkin::kNoteSize;

    // 1. Receptor row.
    for (int column = 0; column < 4; ++column) {
        const double zoom = frame.receptor_zoom[static_cast<std::size_t>(column)];
        last_drawn_quads_ += draw_sprite(renderer, skin.receptor(column, frame.beat),
                                         field.column_x(column, field_left), receptor_y,
                                         note_size * zoom);
    }

    // 2. Hold/roll bodies and end caps.
    for (const NoteRenderItem& item : items) {
        if (!item.has_body) {
            continue;
        }
        double head_y = field.screen_y(item.head_offset);
        const double tail_y = field.screen_y(item.tail_offset);
        // While a hit hold is being held, the head has scrolled past the
        // receptor; clamp the leading edge to the receptor so the body recedes
        // (OpenITG draws the remaining, still-held portion of the hold).
        if (item.held) {
            if (field.config().direction == ScrollDirection::Down) {
                head_y = std::min(head_y, receptor_y);
            } else {
                head_y = std::max(head_y, receptor_y);
            }
        }
        const HoldSprites hold = skin.hold(item.type, item.held, quantization_of(item));
        last_drawn_quads_ += draw_hold(renderer, hold, field.column_x(item.column, field_left),
                                       head_y, tail_y, static_cast<double>(screen_h));
    }

    // 3. Heads (taps, hold heads, roll heads). A hold being held keeps its head
    // pinned in the receptor (OpenITG NoteDisplay draws it at y offset 0).
    for (const NoteRenderItem& item : items) {
        if (item.type == NoteType::Mine) {
            continue;
        }
        const double y = item.held ? receptor_y : field.screen_y(item.head_offset);
        last_drawn_quads_ += draw_sprite(
            renderer, skin.head(item.type, item.column, quantization_of(item), frame.beat),
            field.column_x(item.column, field_left), y, note_size);
    }

    // 4. Mines, drawn last so they sit on top; layer by layer so each layer's
    // texture binds once.
    for (const SkinSprite& layer : skin.mine(frame.music_seconds)) {
        for (const NoteRenderItem& item : items) {
            if (item.type != NoteType::Mine) {
                continue;
            }
            last_drawn_quads_ += draw_sprite(renderer, layer, field.column_x(item.column, field_left),
                                             field.screen_y(item.head_offset), note_size);
        }
    }

    // 5. Explosions over the receptors, in the Cel explosion actor order: hold
    // glow, tap flashes, then the mine burst.
    for (int column = 0; column < 4; ++column) {
        const auto index = static_cast<std::size_t>(column);
        const double x = field.column_x(column, field_left);
        if (frame.hold_explosion[index]) {
            last_drawn_quads_ +=
                draw_sprite(renderer, skin.hold_explosion(column), x, receptor_y, note_size);
        }
        const TapExplosionState& tap = frame.tap_explosion[index];
        last_drawn_quads_ += draw_sprite(
            renderer, skin.tap_explosion(column, tap.window, tap.elapsed, tap.duration), x,
            receptor_y, note_size);
        last_drawn_quads_ += draw_sprite(
            renderer, skin.mine_explosion(frame.mine_explosion_elapsed[index]), x, receptor_y,
            note_size);
    }
}

} // namespace td
