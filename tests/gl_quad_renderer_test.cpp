#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "render/geometry.hpp"
#include "render/gl_quad_renderer.hpp"
#include "render/texture.hpp"
#include "render/theme.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

using blaze4k::Color;
using blaze4k::QuadVertex;
using blaze4k::Rect;
using blaze4k::UVRect;
using blaze4k::Vec2;

bool same_colour(const QuadVertex& v, const Color& c) {
    return v.r == c.r && v.g == c.g && v.b == c.b && v.a == c.a;
}

bool same_vertex(const QuadVertex& a, const QuadVertex& b) {
    return a.x == b.x && a.y == b.y && a.u == b.u && a.v == b.v && a.r == b.r && a.g == b.g &&
           a.b == b.b && a.a == b.a;
}

bool approx_equal(float a, float b) {
    return std::fabs(a - b) <= 1e-4f;
}

// Vertex index -> corner index (TL=0, TR=1, BR=2, BL=3) for the TL,TR,BR / TL,BR,BL split.
constexpr std::array<int, 6> kCornerOf = {0, 1, 2, 0, 2, 3};

// Reference copy of the axis-aligned part of GlQuadRenderer::append_quad
// (src/render/gl_quad_renderer.cpp: corner build + push order, no rotation).
// Pins quad_vertices to the same layout, triangle order and premultiply.
std::array<QuadVertex, 6> reference_rect_vertices(const Rect& rect, const UVRect& uv, Color color) {
    const float x0 = rect.x;
    const float y0 = rect.y;
    const float x1 = rect.x + rect.w;
    const float y1 = rect.y + rect.h;

    const Color pm = blaze4k::premultiply(color);

    QuadVertex top_left{x0, y0, uv.u0, uv.v0, pm.r, pm.g, pm.b, pm.a};
    QuadVertex top_right{x1, y0, uv.u1, uv.v0, pm.r, pm.g, pm.b, pm.a};
    QuadVertex bottom_right{x1, y1, uv.u1, uv.v1, pm.r, pm.g, pm.b, pm.a};
    QuadVertex bottom_left{x0, y1, uv.u0, uv.v1, pm.r, pm.g, pm.b, pm.a};

    return {top_left, top_right, bottom_right, top_left, bottom_right, bottom_left};
}

std::array<Vec2, 4> rect_corners(const Rect& rect) {
    return {Vec2{rect.x, rect.y}, Vec2{rect.x + rect.w, rect.y},
            Vec2{rect.x + rect.w, rect.y + rect.h}, Vec2{rect.x, rect.y + rect.h}};
}

void test_corner_order() {
    const std::array<Vec2, 4> corners = {Vec2{10.0f, 20.0f}, Vec2{110.0f, 25.0f},
                                         Vec2{105.0f, 80.0f}, Vec2{5.0f, 75.0f}};
    const Color white{};
    const auto quad = blaze4k::quad_vertices(corners, UVRect{}, {white, white, white, white});
    for (std::size_t i = 0; i < quad.size(); ++i) {
        const Vec2& expected = corners[static_cast<std::size_t>(kCornerOf[i])];
        TEST_CHECK(quad[i].x == expected.x);
        TEST_CHECK(quad[i].y == expected.y);
    }
    std::cout << "  - corners emitted as TL,TR,BR / TL,BR,BL ok.\n";
}

void test_per_vertex_colour_premultiplied() {
    const std::array<Vec2, 4> corners = {Vec2{0.0f, 0.0f}, Vec2{10.0f, 0.0f}, Vec2{10.0f, 10.0f},
                                         Vec2{0.0f, 10.0f}};
    const std::array<Color, 4> colours = {Color{1.0f, 0.0f, 0.0f, 0.5f}, Color{0.0f, 1.0f, 0.0f, 1.0f},
                                          Color{0.0f, 0.0f, 1.0f, 0.25f},
                                          Color{1.0f, 1.0f, 1.0f, 0.0f}};
    const auto quad = blaze4k::quad_vertices(corners, UVRect{}, colours);
    for (std::size_t i = 0; i < quad.size(); ++i) {
        const Color expected =
            blaze4k::premultiply(colours[static_cast<std::size_t>(kCornerOf[i])]);
        TEST_CHECK(same_colour(quad[i], expected));
    }
    // TL is half-alpha red -> (0.5, 0, 0, 0.5).
    TEST_CHECK(same_colour(quad[0], Color{0.5f, 0.0f, 0.0f, 0.5f}));
    // The fully transparent white BL corner must carry no hidden RGB (#59).
    TEST_CHECK(same_colour(quad[5], Color{0.0f, 0.0f, 0.0f, 0.0f}));
    std::cout << "  - per-vertex colours premultiplied ok.\n";
}

void test_uv_mapping_repeat_range() {
    const std::array<Vec2, 4> corners = {Vec2{0.0f, 0.0f}, Vec2{1280.0f, 0.0f},
                                         Vec2{1280.0f, 720.0f}, Vec2{0.0f, 720.0f}};
    const Color white{};
    const std::array<Color, 4> colours = {white, white, white, white};

    const auto check_uv = [&](const UVRect& uv) {
        const auto quad = blaze4k::quad_vertices(corners, uv, colours);
        const std::array<std::array<float, 2>, 4> expected = {
            std::array<float, 2>{uv.u0, uv.v0}, std::array<float, 2>{uv.u1, uv.v0},
            std::array<float, 2>{uv.u1, uv.v1}, std::array<float, 2>{uv.u0, uv.v1}};
        for (std::size_t i = 0; i < quad.size(); ++i) {
            const auto& e = expected[static_cast<std::size_t>(kCornerOf[i])];
            TEST_CHECK(quad[i].u == e[0]);
            TEST_CHECK(quad[i].v == e[1]);
        }
        return quad;
    };

    // Repeat range above 1 is passed through unclamped (texture tiles 4 x 2.5 times).
    const auto tiled = check_uv(UVRect{0.0f, 0.0f, 4.0f, 2.5f});
    TEST_CHECK(tiled[1].u == 4.0f);  // TR
    TEST_CHECK(tiled[2].v == 2.5f);  // BR
    // Negative / offset range (scrolling scanlines) round-trips exactly.
    const auto scrolled = check_uv(UVRect{-0.5f, 0.25f, 3.5f, 8.25f});
    TEST_CHECK(scrolled[0].u == -0.5f);
    TEST_CHECK(scrolled[5].v == 8.25f);
    std::cout << "  - UVs map per corner and repeat ranges stay unclamped ok.\n";
}

void test_parallelogram_skew() {
    // Intended consumer usage: a rect sheared by a theme skew (tan of the angle),
    // with the top edge offset by h * skew.
    const Rect rect{100.0f, 50.0f, 200.0f, 30.0f};
    const float dx = rect.h * blaze4k::theme::skew::kRows;
    const std::array<Vec2, 4> corners = {Vec2{rect.x + dx, rect.y},
                                         Vec2{rect.x + rect.w + dx, rect.y},
                                         Vec2{rect.x + rect.w, rect.y + rect.h},
                                         Vec2{rect.x, rect.y + rect.h}};
    const Color c{0.2f, 0.4f, 0.6f, 1.0f};
    const auto quad = blaze4k::quad_vertices(corners, UVRect{}, {c, c, c, c});

    // Positions are exactly the inputs.
    for (std::size_t i = 0; i < quad.size(); ++i) {
        const Vec2& expected = corners[static_cast<std::size_t>(kCornerOf[i])];
        TEST_CHECK(quad[i].x == expected.x && quad[i].y == expected.y);
    }
    // Top (TL->TR) and bottom (BL->BR) edges are parallel and equal in length.
    const float top_dx = quad[1].x - quad[0].x;
    const float top_dy = quad[1].y - quad[0].y;
    const float bottom_dx = quad[4].x - quad[5].x;
    const float bottom_dy = quad[4].y - quad[5].y;
    TEST_CHECK(approx_equal(top_dx, bottom_dx));
    TEST_CHECK(approx_equal(top_dy, bottom_dy));
    // And it is actually slanted: the top-left sits right of the bottom-left.
    TEST_CHECK(quad[0].x > quad[5].x);
    TEST_CHECK(approx_equal(quad[0].x - quad[5].x, dx));
    std::cout << "  - theme::skew parallelogram corners ok.\n";
}

void test_layout_parity_with_append_quad() {
    const std::array<Rect, 3> rects = {Rect{0.0f, 0.0f, 64.0f, 64.0f},
                                       Rect{12.5f, 300.25f, 177.75f, 9.0f},
                                       Rect{-40.0f, 700.0f, 1360.0f, 0.5f}};
    const std::array<UVRect, 3> uvs = {UVRect{}, UVRect{0.25f, 0.5f, 0.5f, 0.75f},
                                       UVRect{0.0f, -1.0f, 3.0f, 7.5f}};
    const std::array<Color, 3> colours = {Color{}, Color{0.3f, 0.7f, 0.9f, 0.6f},
                                          Color{1.0f, 1.0f, 1.0f, 0.0f}};
    for (const Rect& rect : rects) {
        for (const UVRect& uv : uvs) {
            for (const Color& c : colours) {
                const auto expected = reference_rect_vertices(rect, uv, c);
                const auto actual = blaze4k::quad_vertices(rect_corners(rect), uv, {c, c, c, c});
                for (std::size_t i = 0; i < expected.size(); ++i) {
                    TEST_CHECK(same_vertex(actual[i], expected[i]));
                }
            }
        }
    }
    std::cout << "  - layout parity with append_quad (axis-aligned) ok.\n";
}

void test_headless_noop() {
    blaze4k::GlQuadRenderer renderer;
    renderer.begin(1280, 720);
    const std::array<Vec2, 4> corners = {Vec2{0.0f, 0.0f}, Vec2{10.0f, 0.0f}, Vec2{10.0f, 10.0f},
                                         Vec2{0.0f, 10.0f}};
    const Color c{};
    const blaze4k::Texture invalid;
    renderer.draw_quad_points(corners, invalid, UVRect{}, {c, c, c, c});
    const blaze4k::Texture default_texture{};
    renderer.set_blend_mode(blaze4k::BlendMode::Add);
    renderer.draw_quad_points(corners, default_texture, UVRect{0.0f, 0.0f, 4.0f, 4.0f}, {c, c, c, c});
    renderer.end();
    TEST_CHECK(!renderer.is_initialized());
    std::cout << "  - uninitialized renderer draw_quad_points is a no-op ok.\n";
}

} // namespace

int main() {
    std::cout << "[gl_quad_renderer_test] Running general-quad vertex tests...\n";
    test_corner_order();
    test_per_vertex_colour_premultiplied();
    test_uv_mapping_repeat_range();
    test_parallelogram_skew();
    test_layout_parity_with_append_quad();
    test_headless_noop();
    std::cout << "[gl_quad_renderer_test] All tests passed!\n";
    return 0;
}
