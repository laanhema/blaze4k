#include "screens/select_placeholder_screen.hpp"

#include <algorithm>
#include <iostream>

#include "render/bitmap_font.hpp"
#include "render/gl_quad_renderer.hpp"

namespace td {

namespace {

constexpr Color kTitleColor{0.86f, 0.93f, 1.00f, 1.0f};
constexpr Color kSubColor{0.90f, 0.75f, 0.30f, 1.0f};
constexpr Color kHintColor{0.60f, 0.66f, 0.78f, 1.0f};

} // namespace

void SelectPlaceholderScreen::enter(ScreenContext& /*ctx*/) {
    std::cout << "[SelectPlaceholder] Song Select not implemented yet (C3)\n";
}

void SelectPlaceholderScreen::update(ScreenContext& /*ctx*/, double /*fixed_dt*/,
                                     const std::vector<InputEvent>& events) {
    // Back is handled centrally by the ScreenManager. Confirm has nowhere to go
    // until C3 lands.
    for (const InputEvent& event : events) {
        if (event.pressed && event.action == GameAction::Confirm) {
            std::cout << "[SelectPlaceholder] Confirm ignored (C3 pending)\n";
            break;
        }
    }
}

void SelectPlaceholderScreen::render(ScreenContext& /*ctx*/, GlQuadRenderer& renderer, int w,
                                     int h) {
    if (w <= 0 || h <= 0) {
        return;
    }

    const float width = static_cast<float>(w);
    const float height = static_cast<float>(h);

    const float title_pixel = std::max(2.0f, width * 0.005f);
    draw_text_centered(renderer, "SONG SELECT", width * 0.5f, height * 0.30f, title_pixel,
                       kTitleColor);
    draw_text_centered(renderer, "COMING SOON", width * 0.5f, height * 0.46f, 3.0f, kSubColor);
    draw_text_centered(renderer, "[BACK] TO TITLE", width * 0.5f, height * 0.80f, 3.0f, kHintColor);
}

} // namespace td