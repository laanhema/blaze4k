#include "screens/title_screen.hpp"

#include <iostream>

#include "gameplay/noteskin.hpp"
#include "render/gl_quad_renderer.hpp"
#include "render/theme_layout.hpp"
#include "render/theme_textures.hpp"
#include "render/ttf_font.hpp"
#include "screens/screen_manager.hpp"
#include "screens/title_art.hpp"

namespace blaze4k {

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

void TitleScreen::render(ScreenContext& ctx, GlQuadRenderer& renderer, int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }

    namespace layout = theme::layout;
    const theme::LayoutScale L = theme::layout_scale(w, h);

    if (ctx.theme != nullptr) {
        title_art::draw_backdrop(*ctx.theme, renderer, w, h);
        title_art::draw_centred_sprite(*ctx.theme, renderer, "logo", L, layout::kLogoTop);
        title_art::draw_centred_sprite(*ctx.theme, renderer, "subtitle", L, layout::kSubtitleTop);
    }

    if (ctx.noteskin != nullptr) {
        const float box = title_art::arrow_box(L);
        for (int column = 0; column < 4; ++column) {
            const Vec2 centre = title_art::arrow_centre(L, column);
            draw_skin_sprite(
                renderer,
                ctx.noteskin->head(NoteType::Tap, column, title_art::arrow_quantization(column),
                                   title_art::kArrowBeat),
                centre.x, centre.y, box);
        }
    }

    if (ctx.theme != nullptr && title_art::prompt_visible(blink_seconds_)) {
        title_art::draw_centred_sprite(*ctx.theme, renderer, "press_start", L,
                                       layout::kPressStartTop);
    }

    if (ctx.text != nullptr) {
        const float top = title_art::footer_text_top(L, ctx.text->line_height(theme::text::kFooter));
        ctx.text->draw(renderer, title_art::footer_left_text(), L.x(layout::kFooterPadX), top,
                       theme::text::kFooter, TextAlign::Left);
        ctx.text->draw(renderer, title_art::footer_right_text(),
                       L.x(layout::kRefWidth - layout::kFooterPadX), top, theme::text::kFooter,
                       TextAlign::Right);
    }

    if (ctx.theme != nullptr) {
        title_art::draw_scanlines(*ctx.theme, renderer, w, h, L.s);
    }
}

} // namespace blaze4k
