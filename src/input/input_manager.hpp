#pragma once

#include <vector>
#include <unordered_map>
#include <SDL3/SDL.h>
#include "input/input_event.hpp"

namespace td {

class InputManager {
public:
    InputManager();
    ~InputManager();

    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;
    InputManager(InputManager&& other) noexcept;
    InputManager& operator=(InputManager&& other) noexcept;

    void handle_sdl_event(const SDL_Event& event);
    std::vector<InputEvent> poll_events();

    [[nodiscard]] bool is_action_down(GameAction action) const;
    [[nodiscard]] GameAction action_for_key(SDL_Keycode key) const;
    [[nodiscard]] size_t connected_gamepads_count() const { return gamepads_.size(); }

    void bind_key(SDL_Keycode key, GameAction action);
    void bind_gamepad_button(uint8_t button, GameAction action);
    void reset_to_defaults();

    void shutdown();

private:
    void setup_default_mappings();
    void clear_action_states();
    void on_gamepad_added(SDL_JoystickID joystick_id);
    void on_gamepad_removed(SDL_JoystickID joystick_id);

    std::unordered_map<SDL_Keycode, GameAction> key_map_;
    std::unordered_map<uint8_t, GameAction> gamepad_button_map_;
    std::unordered_map<SDL_JoystickID, SDL_Gamepad*> gamepads_;
    std::unordered_map<GameAction, bool> action_states_;
    std::vector<InputEvent> event_queue_;
};

} // namespace td
