#include "screens/input_remap_screen.hpp"

#include <span>
#include <string>

#include <SDL3/SDL.h>

#include "data/config.hpp"
#include "input/input_manager.hpp"
#include "render/gl_quad_renderer.hpp"
#include "render/theme_layout.hpp"
#include "render/theme_textures.hpp"
#include "screens/select_art.hpp"
#include "screens/setup_art.hpp"

namespace blaze4k {

void InputRemapScreen::enter(ScreenContext& ctx) {
    model_ = input_remap_from_config(ctx.config != nullptr ? *ctx.config : GameConfig{});
    reset_selected_ = false;
    if (ctx.input != nullptr) {
        ctx.input->set_capture_mode(false);
    }
}

void InputRemapScreen::start_capture(ScreenContext& ctx) {
    model_.capturing = true;
    model_.message.clear();
    if (ctx.input != nullptr) {
        ctx.input->set_capture_mode(true);
    }
}

void InputRemapScreen::cancel_capture(ScreenContext& ctx) {
    model_.capturing = false;
    if (ctx.input != nullptr) {
        ctx.input->set_capture_mode(false);
    }
}

void InputRemapScreen::commit(ScreenContext& ctx) {
    if (ctx.config == nullptr) {
        return;
    }
    input_remap_apply(model_, ctx.config->input);
    if (ctx.input != nullptr) {
        ctx.input->apply_bindings(ctx.config->input);
    }
}

void InputRemapScreen::update(ScreenContext& ctx, double /*fixed_dt*/,
                              const std::vector<InputEvent>& events) {
    if (!model_.capturing) {
        const int last_binding_row = static_cast<int>(model_.rows.size()) - 1;
        for (const InputEvent& event : events) {
            if (!event.pressed) {
                continue;
            }
            switch (event.action) {
                case GameAction::Up:
                    model_.message.clear(); // navigating dismisses a stale conflict notice
                    if (reset_selected_) {
                        reset_selected_ = false;
                    } else {
                        input_remap_move_row(model_, -1);
                    }
                    break;
                case GameAction::Down:
                    model_.message.clear();
                    if (reset_selected_) {
                        break;
                    }
                    if (model_.row >= last_binding_row) {
                        reset_selected_ = true; // step onto the trailing RESET row
                    } else {
                        input_remap_move_row(model_, +1);
                    }
                    break;
                case GameAction::Confirm:
                case GameAction::Right:
                    if (reset_selected_) {
                        input_remap_reset(model_);
                        commit(ctx);
                        reset_selected_ = false;
                    } else {
                        start_capture(ctx);
                    }
                    break;
                default:
                    break; // Back is owned by handle_back()
            }
        }
        return;
    }

    // Capture mode: the next physical key/button becomes the binding. Escape and
    // pad-Back are reserved; they cancel capture rather than binding.
    for (const InputEvent& event : events) {
        if (!event.pressed) {
            continue;
        }
        if (event.device == DeviceType::Keyboard &&
            event.raw_code == static_cast<uint32_t>(SDLK_ESCAPE)) {
            cancel_capture(ctx);
            return;
        }
        if (event.device == DeviceType::Gamepad &&
            event.raw_code == static_cast<uint32_t>(SDL_GAMEPAD_BUTTON_BACK)) {
            cancel_capture(ctx);
            return;
        }

        std::string name;
        if (event.device == DeviceType::Keyboard) {
            const char* sdl_name = SDL_GetKeyName(static_cast<SDL_Keycode>(event.raw_code));
            if (sdl_name != nullptr) {
                name = sdl_name;
            }
        } else {
            const char* sdl_name =
                SDL_GetGamepadStringForButton(static_cast<SDL_GamepadButton>(event.raw_code));
            if (sdl_name != nullptr) {
                name = sdl_name;
            }
        }
        if (name.empty()) {
            continue;
        }

        const RemapStatus status = input_remap_assign(model_, name);
        if (status == RemapStatus::Conflict) {
            continue; // rejected: keep capturing so the player can try another input
        }
        model_.capturing = false;
        if (ctx.input != nullptr) {
            ctx.input->set_capture_mode(false);
        }
        commit(ctx);
        return;
    }
}

void InputRemapScreen::render(ScreenContext& ctx, GlQuadRenderer& renderer, int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }

    const theme::LayoutScale L = theme::layout_scale(w, h);
    const ThemeTextures* theme = ctx.theme;
    TextRenderer* text = ctx.text;
    setup_art::draw_chrome(theme, text, renderer, L, w, h, setup_art::kRemapTitle);
    setup_art::draw_message_chip(theme, text, renderer, L, model_.message);
    setup_art::draw_remap_table(theme, text, renderer, L, model_, reset_selected_);

    std::span<const setup_art::HintItem> legend = setup_art::kRemapBrowseHint;
    if (model_.capturing) {
        legend = setup_art::kRemapCaptureHint;
    } else if (reset_selected_) {
        legend = setup_art::kRemapResetHint;
    }
    setup_art::draw_hint_bar(theme, text, renderer, L, w, legend);

    if (theme != nullptr) {
        select_art::draw_scanlines(*theme, renderer, w, h, L);
    }
}

void InputRemapScreen::exit(ScreenContext& ctx) {
    if (ctx.input != nullptr) {
        ctx.input->set_capture_mode(false);
    }
}

bool InputRemapScreen::handle_back(ScreenContext& ctx) {
    if (model_.capturing) {
        cancel_capture(ctx);
        return true;
    }
    return false;
}

} // namespace blaze4k
