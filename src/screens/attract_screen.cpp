#include "screens/attract_screen.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

#include "render/bitmap_font.hpp"
#include "render/gl_quad_renderer.hpp"

namespace td {

namespace {

constexpr Color kBackdropColor{0.03f, 0.04f, 0.08f, 1.0f};
constexpr Color kLogoColor{0.80f, 0.90f, 1.00f, 1.0f};
constexpr Color kReceptorColor{0.35f, 0.42f, 0.55f, 1.0f};
constexpr Color kActiveReceptorColor{1.00f, 0.85f, 0.30f, 1.0f};

[[nodiscard]] Color scale_rgb(Color color, float factor) {
    return Color{color.r * factor, color.g * factor, color.b * factor, color.a};
}

} // namespace

void AttractScreen::enter(ScreenContext& /*ctx*/) {
    phase_seconds_ = 0.0;
    std::cout << "[AttractScreen] title loop active (any Confirm exits)\n";
}

void AttractScreen::update(ScreenContext& /*ctx*/, double fixed_dt,
                           const std::vector<InputEvent>& /*events*/) {
    // Confirm is handled centrally by the ScreenManager; this screen just advances
    // its animation from the injected fixed timestep.
    phase_seconds_ += fixed_dt;
}

void AttractScreen::render(ScreenContext& /*ctx*/, GlQuadRenderer& renderer, int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }

    const float width = static_cast<float>(w);
    const float height = static_cast<float>(h);

    renderer.draw_quad(Rect{0.0f, 0.0f, width, height}, kBackdropColor);

    // Brightness-pulsed logo (deterministic in the injected fixed_dt).
    const float pulse = 0.65f + 0.35f * static_cast<float>(std::sin(phase_seconds_ * 2.0));
    const float logo_pixel = std::max(2.0f, width * 0.0055f);
    draw_text_centered(renderer, "TUNDRA DANCE", width * 0.5f, height * 0.30f, logo_pixel,
                       scale_rgb(kLogoColor, pulse));

    // Four receptors blinking in sequence.
    const int active = static_cast<int>(phase_seconds_ * 4.0) % 4;
    const float receptor = logo_pixel * 7.0f;
    const float gap = logo_pixel * 3.0f;
    const float row_w = receptor * 4.0f + gap * 3.0f;
    float x = (width - row_w) * 0.5f;
    const float y = height * 0.58f;
    for (int i = 0; i < 4; ++i) {
        const bool lit = i == active;
        const float size = lit ? receptor * 1.15f : receptor;
        const float offset = (size - receptor) * 0.5f;
        const Color color = lit ? kActiveReceptorColor : kReceptorColor;
        renderer.draw_quad(Rect{x - offset, y - offset, size, size}, color);
        x += receptor + gap;
    }

    draw_text_centered(renderer, "PRESS START", width * 0.5f, height * 0.78f, 3.0f,
                       scale_rgb(kLogoColor, pulse));
}

} // namespace td