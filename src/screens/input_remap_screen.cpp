#include "screens/input_remap_screen.hpp"

#include <algorithm>
#include <string>

#include <SDL3/SDL.h>

#include "data/config.hpp"
#include "input/input_manager.hpp"
#include "render/bitmap_font.hpp"
#include "render/gl_quad_renderer.hpp"

namespace td {

namespace {

constexpr Color kBackdrop{0.05f, 0.07f, 0.12f, 1.0f};
constexpr Color kTitleColor{0.86f, 0.93f, 1.00f, 1.0f};
constexpr Color kAccentColor{1.00f, 0.92f, 0.35f, 1.0f};
constexpr Color kTextColor{0.82f, 0.87f, 0.95f, 1.0f};
constexpr Color kHintColor{0.60f, 0.66f, 0.78f, 1.0f};
constexpr Color kHighlight{0.16f, 0.20f, 0.34f, 1.0f};
constexpr Color kResetColor{0.90f, 0.55f, 0.45f, 1.0f};

} // namespace

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

void InputRemapScreen::render(ScreenContext& /*ctx*/, GlQuadRenderer& renderer, int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }

    const float width = static_cast<float>(w);
    const float height = static_cast<float>(h);
    renderer.draw_quad(Rect{0.0f, 0.0f, width, height}, kBackdrop);

    const float title_pixel = std::max(2.5f, width * 0.0045f);
    draw_text(renderer, "REMAP INPUT", width * 0.05f, height * 0.05f, title_pixel, kTitleColor);

    const float text_x = width * 0.08f;
    const float row_h = height * 0.045f;
    const float list_top = height * 0.15f;
    const float list_bottom = height * 0.83f;
    const float name_pixel = std::max(2.0f, width * 0.0032f);

    const int reset_row = static_cast<int>(model_.rows.size());
    const int total_rows = reset_row + 1;

    // More rows exist than fit at 720p, so scroll a window that keeps the
    // selection visible: every binding and the trailing RESET row stay reachable.
    const int visible_rows = std::max(1, static_cast<int>((list_bottom - list_top) / row_h));
    const int selected_row =
        model_.capturing ? model_.row : (reset_selected_ ? reset_row : model_.row);
    int first_row = selected_row - visible_rows / 2;
    first_row = std::clamp(first_row, 0, std::max(0, total_rows - visible_rows));
    const int last_row = std::min(total_rows, first_row + visible_rows);

    // Table layout: ACTION | [DEVICE] | KEY. Column starts come from the widest
    // label over every row (not just the visible window) so they never shift
    // while scrolling.
    float action_col_w = 0.0f;
    float device_col_w = 0.0f;
    for (const RemapRow& remap_row : model_.rows) {
        action_col_w =
            std::max(action_col_w, text_width(remap_action_name(remap_row.action), name_pixel));
        device_col_w = std::max(device_col_w,
                                text_width("[" + remap_device_name(remap_row.device) + "]", name_pixel));
    }
    const float col_gap = text_width("   ", name_pixel);
    const float device_x = text_x + action_col_w + col_gap;
    const float key_x = device_x + device_col_w + col_gap;

    float row_y = list_top;
    for (int i = first_row; i < last_row; ++i) {
        const bool is_reset = i == reset_row;
        const bool selected = model_.capturing ? false : (reset_selected_ ? is_reset : i == model_.row);
        if (selected) {
            renderer.draw_quad(Rect{text_x - width * 0.01f, row_y - row_h * 0.15f,
                                    width * 0.84f, row_h * 0.95f},
                               kHighlight);
        }

        if (is_reset) {
            draw_text(renderer, "RESET TO DEFAULTS", text_x, row_y, name_pixel,
                      selected ? kAccentColor : kResetColor);
        } else {
            const RemapRow& remap_row = model_.rows[static_cast<std::size_t>(i)];
            const Color color = selected ? kAccentColor : kTextColor;
            draw_text(renderer, remap_action_name(remap_row.action), text_x, row_y, name_pixel,
                      color);
            draw_text(renderer, "[" + remap_device_name(remap_row.device) + "]", device_x, row_y,
                      name_pixel, color);
            draw_text(renderer, remap_row_value_text(model_, i), key_x, row_y, name_pixel, color);
        }
        row_y += row_h;
    }

    if (!model_.message.empty()) {
        draw_text(renderer, model_.message, text_x, list_bottom + row_h * 0.1f, name_pixel,
                  kAccentColor);
    }

    const char* footer = model_.capturing
                             ? "PRESS A KEY OR PAD BUTTON (ESC/CANCEL TO ABORT)"
                             : "[UP/DOWN] SELECT  [ENTER] REBIND  [BACK] EXIT";
    draw_text_centered(renderer, footer, width * 0.5f, height * 0.9f,
                       std::max(2.0f, width * 0.0028f), kHintColor);
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

} // namespace td
