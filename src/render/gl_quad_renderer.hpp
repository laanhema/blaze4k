#pragma once

#include <array>
#include <vector>
#include "render/geometry.hpp"
#include "render/texture.hpp"

namespace blaze4k {

// One vertex of the quad batch: position (pixels), texture coordinate, and
// premultiplied RGBA colour. The field order is the GL attribute layout.
struct QuadVertex {
    float x;
    float y;
    float u;
    float v;
    float r;
    float g;
    float b;
    float a;
};
static_assert(sizeof(QuadVertex) == 8 * sizeof(float), "QuadVertex must stay 8 tightly packed floats");

// Builds the six vertices (two triangles: TL,TR,BR and TL,BR,BL) of a general
// quad. `corners` and `colours` are in TL, TR, BR, BL order; UVs map
// TL=(u0,v0), TR=(u1,v0), BR=(u1,v1), BL=(u0,v1) and are passed through
// unclamped (a Repeat-wrapped texture tiles when the range exceeds 1).
// Colours are straight alpha and are premultiplied per vertex (#59), exactly
// like append_quad. Pure (no GL).
[[nodiscard]] std::array<QuadVertex, 6> quad_vertices(const std::array<Vec2, 4>& corners,
                                                      const UVRect& uv,
                                                      const std::array<Color, 4>& colours);

// Batched 2D textured-quad renderer for OpenGL 3.3 core.
//
// Coordinates are pixels with a top-left origin and y growing downward, matching
// the gameplay layout math in `src/gameplay/`. Every call is guarded so that a
// headless (uninitialized) renderer is a safe no-op.
//
// Colours passed in are straight alpha; internally textures and vertex colours
// are premultiplied (see `premultiply`, `premultiply_alpha`) and the blend
// functions are the premultiplied equivalents, so filtered edges never pick up
// the hidden RGB of transparent texels (#59).
//
// `draw_quad_points` is the general-quad path (arbitrary corners, per-corner
// colour); it shares the vertex layout and premultiplication with the
// axis-aligned calls via `quad_vertices`.
class GlQuadRenderer {
public:
    GlQuadRenderer() = default;
    ~GlQuadRenderer();

    GlQuadRenderer(const GlQuadRenderer&) = delete;
    GlQuadRenderer& operator=(const GlQuadRenderer&) = delete;

    bool init();
    void shutdown();
    [[nodiscard]] bool is_initialized() const { return initialized_; }

    // Sets the viewport + orthographic projection for one frame.
    void begin(int framebuffer_width, int framebuffer_height);

    // Solid white quad tinted by `color`.
    void draw_quad(const Rect& rect, Color color);

    // Textured quad tinted by `color`.
    void draw_textured_quad(const Rect& rect, const Texture& texture, const UVRect& uv, Color color);

    // Textured quad rotated `radians` about its center (clockwise on screen,
    // since y grows downward). Lets one-direction art serve every column.
    void draw_textured_quad(const Rect& rect, const Texture& texture, const UVRect& uv, Color color,
                            float radians);

    // General quad from four corners (TL, TR, BR, BL; any convex quad, e.g. a
    // parallelogram from a theme::skew), each with its own straight-alpha colour.
    // An invalid `texture` draws solid (the white texture), like draw_textured_quad.
    // Colour and UV are interpolated per triangle (split along TL-BR): a linear
    // gradient (TL==TR and BL==BR, or TL==BL and TR==BR) is exact only when its two
    // constant-colour edges are parallel (rects, parallelograms); on a general
    // convex quad, with four distinct colours, or a non-parallelogram with texture,
    // the TL-BR diagonal shows.
    void draw_quad_points(const std::array<Vec2, 4>& corners, const Texture& texture,
                          const UVRect& uv, const std::array<Color, 4>& colours);

    // Blend mode for subsequent quads; flushes the batch when it changes.
    // `begin` resets it to `BlendMode::Alpha`.
    void set_blend_mode(BlendMode mode);

    // Flushes any pending geometry.
    void end();

private:
    using Vertex = QuadVertex;

    void flush();
    void append_quad(const Rect& rect, const UVRect& uv, Color color, float radians = 0.0f);
    void build_projection(int width, int height);
    void bind_texture(unsigned int id);

    bool initialized_ = false;
    unsigned int program_ = 0;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    int projection_location_ = -1;
    int texture_location_ = -1;

    Texture white_;

    std::vector<Vertex> vertices_;
    unsigned int bound_texture_ = 0;
    BlendMode blend_mode_ = BlendMode::Alpha;
    float projection_[16] = {0.0f};
};

} // namespace blaze4k
