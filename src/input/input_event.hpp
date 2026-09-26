#pragma once

#include <cstdint>
#include <string_view>

namespace td {

enum class GameAction {
    // 4 rhythm gameplay panels
    Left,
    Down,
    Up,
    Right,

    // Menu navigation & control
    Confirm,
    Back,
    MenuUp,
    MenuDown,
    MenuLeft,
    MenuRight,

    // In-screen options overlay (C4): open/close only; navigation reuses the
    // directional actions and Confirm/Back, all of which are pad-mapped.
    Options,

    None
};

constexpr std::string_view action_to_string(GameAction action) {
    switch (action) {
        case GameAction::Left: return "Left";
        case GameAction::Down: return "Down";
        case GameAction::Up: return "Up";
        case GameAction::Right: return "Right";
        case GameAction::Confirm: return "Confirm";
        case GameAction::Back: return "Back";
        case GameAction::MenuUp: return "MenuUp";
        case GameAction::MenuDown: return "MenuDown";
        case GameAction::MenuLeft: return "MenuLeft";
        case GameAction::MenuRight: return "MenuRight";
        case GameAction::Options: return "Options";
        case GameAction::None: return "None";
    }
    return "Unknown";
}

enum class DeviceType {
    Keyboard,
    Gamepad
};

struct InputEvent {
    GameAction action = GameAction::None;
    bool pressed = false;
    uint64_t timestamp_ns = 0; // Direct SDL3 nanosecond timestamp
    DeviceType device = DeviceType::Keyboard;
    int device_id = 0;
    uint32_t raw_code = 0; // SDL_Keycode or SDL_GamepadButton
};

} // namespace td
