#include "render/background_renderer.hpp"

#include <iostream>

#include "render/gl_quad_renderer.hpp"

namespace blaze4k {

namespace {

// Blaze 4k's own presentation constants (unsourced; no OpenITG parity
// requirement). Effective background brightness is bounded by the dim over the
// black clear plus the scrim, while notes/HUD draw afterwards at full
// brightness. Tunable until D2.
constexpr float kBackgroundDim = 0.35f;
constexpr Color kBackgroundOverlay{0.0f, 0.0f, 0.0f, 0.30f};
// Safety fill when no image is available (drawn under the scrim).
constexpr Color kFallbackSolid{0.02f, 0.03f, 0.06f, 1.0f};

} // namespace

BackgroundRenderer::~BackgroundRenderer() {
    shutdown();
}

void BackgroundRenderer::load(const std::string& path) {
    image_.destroy();

    if (path.empty()) {
        std::cerr << "[BackgroundRenderer] No background path; scrim-only fallback\n";
        return;
    }

    std::cout << "[BackgroundRenderer] background '" << path << "'\n";
    image_ = Texture::from_file(path);
    if (image_.valid()) {
        std::cout << "[BackgroundRenderer] background texture " << image_.width() << "x"
                  << image_.height() << "\n";
    } else {
        std::cerr << "[BackgroundRenderer] background texture unavailable "
                     "(headless/no GL or decode failure); scrim-only fallback\n";
    }
}

UVRect BackgroundRenderer::cover_uv(int screen_w, int screen_h, int tex_w, int tex_h) {
    if (screen_w <= 0 || screen_h <= 0 || tex_w <= 0 || tex_h <= 0) {
        return UVRect{};
    }

    const double screen_aspect = static_cast<double>(screen_w) / static_cast<double>(screen_h);
    const double tex_aspect = static_cast<double>(tex_w) / static_cast<double>(tex_h);

    UVRect uv;
    if (tex_aspect > screen_aspect) {
        // Texture is relatively wider: crop the left/right so the drawn region
        // matches the screen aspect.
        const double visible = screen_aspect / tex_aspect;
        const float inset = static_cast<float>((1.0 - visible) / 2.0);
        uv.u0 = inset;
        uv.u1 = 1.0f - inset;
        uv.v0 = 0.0f;
        uv.v1 = 1.0f;
    } else {
        // Texture is relatively taller (or equal): crop the top/bottom.
        const double visible = tex_aspect / screen_aspect;
        const float inset = static_cast<float>((1.0 - visible) / 2.0);
        uv.v0 = inset;
        uv.v1 = 1.0f - inset;
        uv.u0 = 0.0f;
        uv.u1 = 1.0f;
    }
    return uv;
}

void BackgroundRenderer::render(GlQuadRenderer& renderer, int screen_w, int screen_h) const {
    if (!renderer.is_initialized() || screen_w <= 0 || screen_h <= 0) {
        return;
    }

    const Rect full{0.0f, 0.0f, static_cast<float>(screen_w), static_cast<float>(screen_h)};

    if (image_.valid()) {
        renderer.draw_textured_quad(
            full, image_,
            cover_uv(screen_w, screen_h, image_.width(), image_.height()),
            with_alpha(Color{1.0f, 1.0f, 1.0f, 1.0f}, kBackgroundDim));
    } else {
        renderer.draw_quad(full, kFallbackSolid);
    }

    // Contrast scrim; notes/HUD draw after this and stay at full brightness.
    renderer.draw_quad(full, kBackgroundOverlay);
}

void BackgroundRenderer::shutdown() {
    image_.destroy();
}

} // namespace blaze4k
