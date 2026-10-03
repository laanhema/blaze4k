#include "screens/attract_screen.hpp"

#include <iostream>

#include "gameplay/noteskin.hpp"
#include "render/gl_quad_renderer.hpp"
#include "render/theme_layout.hpp"
#include "render/theme_textures.hpp"
#include "screens/title_art.hpp"

namespace blaze4k {

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

void AttractScreen::render(ScreenContext& ctx, GlQuadRenderer& renderer, int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }

    namespace layout = theme::layout;
    const theme::LayoutScale L = theme::layout_scale(w, h);
    // Brightness pulse and receptor blink, deterministic in the injected fixed_dt.
    const float pulse = title_art::attract_pulse(phase_seconds_);
    const int active = title_art::attract_active_receptor(phase_seconds_);

    if (ctx.theme != nullptr) {
        title_art::draw_backdrop(*ctx.theme, renderer, w, h);
        title_art::draw_centred_sprite(*ctx.theme, renderer, "logo", L, layout::kLogoTop,
                                       Color{pulse, pulse, pulse, 1.0f});
    }

    // Four Cel receptors lighting in sequence: the lit one gets the on-beat
    // flash brightness and a 1.15x zoom, the others rest at the settled grey.
    // The procedural fallback ignores the beat, so dim its tint the same way.
    if (ctx.noteskin != nullptr) {
        const float box = title_art::arrow_box(L);
        for (int column = 0; column < 4; ++column) {
            const bool lit = column == active;
            const double beat = lit ? 0.0 : 0.5;
            SkinSprite sprite = ctx.noteskin->receptor(column, beat);
            sprite.scale = lit ? 1.15f : 1.0f;
            if (!ctx.noteskin->using_cel()) {
                const float brightness = cel_receptor_brightness(beat);
                sprite.tint.r *= brightness;
                sprite.tint.g *= brightness;
                sprite.tint.b *= brightness;
            }
            const Vec2 centre = title_art::arrow_centre(L, column);
            draw_skin_sprite(renderer, sprite, centre.x, centre.y, box);
        }
    }

    if (ctx.theme != nullptr) {
        title_art::draw_centred_sprite(*ctx.theme, renderer, "press_start", L,
                                       layout::kPressStartTop, Color{1.0f, 1.0f, 1.0f, pulse});
        title_art::draw_scanlines(*ctx.theme, renderer, w, h, L.s);
    }
}

} // namespace blaze4k
