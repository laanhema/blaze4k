#pragma once

// Cabinet theme layout scale (#91): maps the 1280x720 reference space that every theme::layout
// value (and every theme::TextStyle size) is written in to window pixels.
//
//  - `s = min(window_height / 720, window_width / 1280)`: the whole 1280x720 layout always fits
//    the window (owner decision on #91). 16:9 windows scale exactly (720p is the identity, 1440p
//    an exact 2x, 1080p 1.5x).
//  - The `1280·s` x `720·s` content column is centred on both axes. Windows wider than 16:9
//    (21:9) show background bands left and right, as the note field already does; windows
//    narrower than 16:9 (16:10, 4:3) show bands above and below. Nothing is ever cropped.
//  - Reference x = 640 lands at w / 2 and reference y = 360 at h / 2 at every size, so the
//    column shares the note field's horizontal centre (NoteField::field_left).
//  - A non-positive width or height (minimised window) gives the identity: s = 1, origin {0, 0}.
//    No division happens on that path.
//  - Outputs are floats with no pixel snapping; rounding is the caller's choice.
//
// Text atlases (TextRenderer, via text_layout_scale), theme texture draw size (ThemeTextures takes
// `s`) and screen placement all use layout_scale_factor(), so there is one definition of `s`.
//
// Pure, GL-free; presentation only, nothing here touches the music clock or judgment path.

#include "render/geometry.hpp"
#include "render/theme.hpp"

namespace blaze4k::theme {

// s = min(h / 720, w / 1280), or 1 when either dimension is <= 0. Pure.
[[nodiscard]] constexpr float layout_scale_factor(int window_width, int window_height) {
    if (window_width <= 0 || window_height <= 0) {
        return 1.0f;
    }
    const float by_height = static_cast<float>(window_height) / layout::kRefHeight;
    const float by_width = static_cast<float>(window_width) / layout::kRefWidth;
    return by_width < by_height ? by_width : by_height;
}

// Reference (1280x720) -> screen pixel mapping for one window size. The default value is the
// identity (720p, or a degenerate window).
struct LayoutScale {
    float s = 1.0f;
    Vec2 origin{};  // screen position of the content column's top-left corner (reference 0,0)

    // A reference length (size, gap, padding) in screen px.
    [[nodiscard]] constexpr float px(float ref_len) const { return ref_len * s; }
    // A reference x / y coordinate in screen px.
    [[nodiscard]] constexpr float x(float ref_x) const { return origin.x + ref_x * s; }
    [[nodiscard]] constexpr float y(float ref_y) const { return origin.y + ref_y * s; }
    [[nodiscard]] constexpr Vec2 point(Vec2 ref) const { return Vec2{x(ref.x), y(ref.y)}; }
    [[nodiscard]] constexpr Rect rect(Rect ref) const {
        return Rect{x(ref.x), y(ref.y), px(ref.w), px(ref.h)};
    }
    // The content column (the whole reference space) as a screen rect.
    [[nodiscard]] constexpr Rect column() const {
        return Rect{origin.x, origin.y, layout::kRefWidth * s, layout::kRefHeight * s};
    }
};

// The layout scale for a `window_width` x `window_height` (pixels) window. Pure.
[[nodiscard]] constexpr LayoutScale layout_scale(int window_width, int window_height) {
    if (window_width <= 0 || window_height <= 0) {
        return LayoutScale{};
    }
    const float s = layout_scale_factor(window_width, window_height);
    return LayoutScale{s, Vec2{(static_cast<float>(window_width) - layout::kRefWidth * s) * 0.5f,
                               (static_cast<float>(window_height) - layout::kRefHeight * s) * 0.5f}};
}

} // namespace blaze4k::theme
