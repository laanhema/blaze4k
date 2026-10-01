#pragma once

namespace blaze4k {

// Straight-alpha RGBA color, components in [0, 1].
struct Color {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

// Pixel-space rectangle with a top-left origin (y grows downward).
struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

// Normalized texture coordinates. (0,0) is top-left of the texture.
struct UVRect {
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;
};

// How a quad combines with what is already drawn. `Add` is StepMania's
// BlendMode_Add (src * alpha + dst): it brightens, never darkens.
enum class BlendMode { Alpha, Add };

[[nodiscard]] constexpr Color with_alpha(Color color, float alpha) {
    color.a = alpha;
    return color;
}

[[nodiscard]] constexpr Color multiply(Color a, Color b) {
    return Color{a.r * b.r, a.g * b.g, a.b * b.b, a.a * b.a};
}

} // namespace blaze4k
