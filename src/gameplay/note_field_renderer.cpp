#include "gameplay/note_field_renderer.hpp"

#include <algorithm>
#include <cmath>

namespace blaze4k {

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

// Draws a hold/roll body from `head_y` toward the tail; the body stops
// `tail_inset_scale` note sizes before the tail and the end cap starts there
// (centred on the tail for Cel), clipped at the head centre (see `layout_hold`).
// The art is authored head-on-top; in reverse it is flipped vertically. A tiled
// body repeats from the body/cap junction toward the head, so the pattern moves
// with the note even while the head is pinned. Pieces entirely outside
// [0, screen_h] are skipped. Returns quads drawn.
int draw_hold(GlQuadRenderer& renderer, const HoldSprites& hold, double x, double head_y,
              double tail_y, bool reverse, double screen_h) {
    if (hold.body == nullptr) {
        return 0;
    }
    int drawn = 0;
    const double width = NoteSkin::kNoteSize * hold.width_scale;
    const HoldLayout layout =
        layout_hold(head_y, tail_y, reverse, width, NoteSkin::kNoteSize * hold.tail_inset_scale,
                    hold.cap != nullptr);
    // One float for the shared body/cap edge, so both quads meet without a seam.
    const float junction_y = static_cast<float>(layout.body_end_y);
    const float left = static_cast<float>(x - width * 0.5);

    if (layout.has_body) {
        const double length = std::abs(layout.body_end_y - head_y);
        const double toward_head = reverse ? 1.0 : -1.0; // screen-y step from tail to head
        const double tile =
            hold.tile_scale > 0.0f ? NoteSkin::kNoteSize * hold.tile_scale : length;

        // Segment [near, far] in distance from the junction; texture v runs 1 at a
        // tile's tail edge to 0 at its head edge.
        for (double near = 0.0; tile > 0.0 && near < length; near += tile) {
            const double far = std::min(near + tile, length);
            const double y_near = layout.body_end_y + toward_head * near;
            const double y_far = layout.body_end_y + toward_head * far;
            const double top = std::min(y_near, y_far);
            const double bottom = std::max(y_near, y_far);
            if (bottom < 0.0 || top > screen_h) {
                continue;
            }
            const float v_near = 1.0f;
            const float v_far = static_cast<float>(1.0 - (far - near) / tile);
            const UVRect uv = reverse ? UVRect{0.0f, v_near, 1.0f, v_far}
                                      : UVRect{0.0f, v_far, 1.0f, v_near};
            const float f_near = near == 0.0 ? junction_y : static_cast<float>(y_near);
            const float f_far = static_cast<float>(y_far);
            const float f_top = std::min(f_near, f_far);
            const float f_bottom = std::max(f_near, f_far);
            const Rect quad{left, f_top, static_cast<float>(width), f_bottom - f_top};
            renderer.draw_textured_quad(quad, *hold.body, uv, hold.tint);
            ++drawn;
        }
    }

    if (layout.has_cap) {
        const double top = std::min(layout.cap_near_y, layout.cap_far_y);
        const double bottom = std::max(layout.cap_near_y, layout.cap_far_y);
        if (bottom >= 0.0 && top <= screen_h) {
            // Unclipped, cap_near_y == body_end_y exactly, so this is junction_y.
            const float f_near = static_cast<float>(layout.cap_near_y);
            const float f_far = static_cast<float>(layout.cap_far_y);
            const float f_top = std::min(f_near, f_far);
            const float f_bottom = std::max(f_near, f_far);
            const Rect quad{left, f_top, static_cast<float>(width), f_bottom - f_top};
            renderer.draw_textured_quad(quad, *hold.cap, layout.cap_uv, hold.tint);
            ++drawn;
        }
    }
    return drawn;
}

} // namespace

HoldLayout layout_hold(double head_y, double tail_y, bool reverse, double cap_size,
                       double tail_inset, bool has_cap) {
    const double d = reverse ? -1.0 : 1.0; // screen-y step from head toward tail
    HoldLayout out;
    const bool cap = has_cap && cap_size > 0.0;
    out.body_end_y = cap ? tail_y - d * tail_inset : tail_y;
    out.has_body = d * (out.body_end_y - head_y) > 0.0;
    if (cap) {
        out.cap_far_y = out.body_end_y + d * cap_size;
        // OpenITG DrawHoldBottomCap (up-scroll), mirrored for reverse: never draw the
        // cap on the head side of the head centre; offset the texture by the clipped length.
        const double clipped = std::max(0.0, d * (head_y - out.body_end_y));
        if (clipped < cap_size) {
            out.has_cap = true;
            out.cap_near_y = out.body_end_y + d * clipped;
            out.cap_v_near = static_cast<float>(clipped / cap_size);
            // Quad top..bottom: the art is head-on-top, flipped vertically in reverse.
            out.cap_uv = reverse ? UVRect{0.0f, 1.0f, 1.0f, out.cap_v_near}
                                 : UVRect{0.0f, out.cap_v_near, 1.0f, 1.0f};
        }
    }
    return out;
}

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
    const bool reverse = field.config().direction == ScrollDirection::Down;

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
            if (reverse) {
                head_y = std::min(head_y, receptor_y);
            } else {
                head_y = std::max(head_y, receptor_y);
            }
        }
        const HoldSprites hold = skin.hold(item.type, item.held, quantization_of(item));
        last_drawn_quads_ += draw_hold(renderer, hold, field.column_x(item.column, field_left),
                                       head_y, tail_y, reverse, static_cast<double>(screen_h));
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

} // namespace blaze4k
