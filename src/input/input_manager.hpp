#pragma once

#include <vector>
#include <unordered_map>
#include <SDL3/SDL.h>
#include "input/input_event.hpp"

namespace blaze4k {

struct InputSettings;

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

    // A pad Back button is classified on release: a short tap emits Back, a hold
    // >= the hold threshold emits Options. This is the fallback for bare USB
    // dance pads that expose no shoulder buttons. Exposed for tests.
    static constexpr uint64_t kBackHoldOptionsNs = 500'000'000ULL; // 500 ms

    void bind_key(SDL_Keycode key, GameAction action);
    void bind_gamepad_button(uint8_t button, GameAction action);
    void reset_to_defaults();

    // C6: rebuild both runtime maps from the persisted binding names (the C2
    // default lists are the single default authority). Names that do not parse
    // are skipped with a warning; an action left with no valid name falls back
    // to its compiled default. Always ends by force-mapping the reserved safety
    // keys (Escape / pad-Back -> Back) so no remap can soft-lock the shell.
    void apply_bindings(const InputSettings& settings);

    // C6: opt-in raw capture. While on, handle_sdl_event emits one raw
    // InputEvent per physical key/button down/up (action = None, raw_code set,
    // SDL nanosecond timestamp preserved verbatim) and bypasses the mapped path
    // and the pad-Back hold synthesis.
    void set_capture_mode(bool on) { capture_mode_ = on; }
    [[nodiscard]] bool capture_mode() const { return capture_mode_; }

    void shutdown();

private:
    void setup_default_mappings();
    void clear_action_states();
    void on_gamepad_added(SDL_JoystickID joystick_id);
    void on_gamepad_removed(SDL_JoystickID joystick_id);
    void handle_gamepad_back(int device_id, bool pressed, uint64_t timestamp_ns);

    std::unordered_map<SDL_Keycode, GameAction> key_map_;
    std::unordered_map<uint8_t, GameAction> gamepad_button_map_;
    std::unordered_map<SDL_JoystickID, SDL_Gamepad*> gamepads_;
    std::unordered_map<GameAction, bool> action_states_;
    std::unordered_map<int, uint64_t> gamepad_back_hold_ns_; // key: device_id
    std::vector<InputEvent> event_queue_;
    bool capture_mode_ = false;
};

} // namespace blaze4k
