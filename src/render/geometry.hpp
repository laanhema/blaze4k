#pragma once

namespace blaze4k {

// Straight-alpha RGBA color, components in [0, 1] (the quad renderer
// premultiplies it internally).
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
// The quad renderer feeds GL a premultiplied source, so `Alpha` is
// glBlendFunc(ONE, ONE_MINUS_SRC_ALPHA) and `Add` is glBlendFunc(ONE, ONE),
// equivalent to StepMania's src*alpha + dst*(1-alpha) and src*alpha + dst.
enum class BlendMode { Alpha, Add };

[[nodiscard]] constexpr Color with_alpha(Color color, float alpha) {
    color.a = alpha;
    return color;
}

[[nodiscard]] constexpr Color multiply(Color a, Color b) {
    return Color{a.r * b.r, a.g * b.g, a.b * b.b, a.a * b.a};
}

// Converts a straight-alpha colour to the premultiplied form the quad renderer feeds GL.
[[nodiscard]] constexpr Color premultiply(Color c) {
    return Color{c.r * c.a, c.g * c.a, c.b * c.a, c.a};
}

} // namespace blaze4k
