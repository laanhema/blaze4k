#include "screens/title_screen.hpp"

#include <algorithm>
#include <iostream>

#include "render/bitmap_font.hpp"
#include "render/gl_quad_renderer.hpp"
#include "screens/screen_manager.hpp"

namespace blaze4k {

namespace {

// Blaze 4k presentation palette (unsourced; no reference parity requirement).
constexpr Color kLogoColor{0.86f, 0.93f, 1.00f, 1.0f};
constexpr Color kPromptColor{1.00f, 0.92f, 0.35f, 1.0f};
constexpr Color kLeftColor{0.95f, 0.25f, 0.75f, 1.0f};
constexpr Color kDownColor{0.30f, 0.85f, 1.00f, 1.0f};
constexpr Color kUpColor{0.45f, 0.95f, 0.40f, 1.0f};
constexpr Color kRightColor{0.95f, 0.55f, 0.20f, 1.0f};

} // namespace

void TitleScreen::enter(ScreenContext& /*ctx*/) {
    blink_seconds_ = 0.0;
    std::cout << "[TitleScreen] logo + \"Press Start\"\n";
}

void TitleScreen::update(ScreenContext& ctx, double fixed_dt,
                         const std::vector<InputEvent>& events) {
    blink_seconds_ += fixed_dt;
    for (const InputEvent& event : events) {
        if (event.pressed && event.action == GameAction::Confirm) {
            if (ctx.manager != nullptr) {
                ctx.manager->transition_to(ScreenId::Select);
            }
            break;
        }
    }
}

void TitleScreen::render(ScreenContext& /*ctx*/, GlQuadRenderer& renderer, int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }

    const float width = static_cast<float>(w);
    const float height = static_cast<float>(h);

    const float logo_pixel = std::max(2.0f, width * 0.006f);
    const float logo_y = height * 0.28f;
    draw_text_centered(renderer, "BLAZE 4K", width * 0.5f, logo_y, logo_pixel, kLogoColor);

    // Receptor row: four solid arrows' worth of color, no asset (PRD logo is an
    // open item; text quads only).
    const float receptor = logo_pixel * 7.0f;
    const float gap = logo_pixel * 3.0f;
    const float row_w = receptor * 4.0f + gap * 3.0f;
    float x = (width - row_w) * 0.5f;
    const float receptor_y = logo_y + 7.0f * logo_pixel + logo_pixel * 4.0f;
    const Color colors[4] = {kLeftColor, kDownColor, kUpColor, kRightColor};
    for (const Color& color : colors) {
        renderer.draw_quad(Rect{x, receptor_y, receptor, receptor}, color);
        x += receptor + gap;
    }

    if ((static_cast<int>(blink_seconds_ * 2.0) % 2) == 0) {
        draw_text_centered(renderer, "PRESS START", width * 0.5f, height * 0.72f, 4.0f,
                           kPromptColor);
    }
}

} // namespace blaze4k