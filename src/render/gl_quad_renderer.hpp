#pragma once

#include <vector>
#include "render/geometry.hpp"
#include "render/texture.hpp"

namespace blaze4k {

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

    // Blend mode for subsequent quads; flushes the batch when it changes.
    // `begin` resets it to `BlendMode::Alpha`.
    void set_blend_mode(BlendMode mode);

    // Flushes any pending geometry.
    void end();

private:
    struct Vertex {
        float x;
        float y;
        float u;
        float v;
        float r;
        float g;
        float b;
        float a;
    };

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
