#include "input/input_manager.hpp"
#include <iostream>

namespace td {

InputManager::InputManager() {
    setup_default_mappings();
}

InputManager::~InputManager() {
    shutdown();
}

InputManager::InputManager(InputManager&& other) noexcept
    : key_map_(std::move(other.key_map_)),
      gamepad_button_map_(std::move(other.gamepad_button_map_)),
      gamepads_(std::move(other.gamepads_)),
      action_states_(std::move(other.action_states_)),
      gamepad_back_hold_ns_(std::move(other.gamepad_back_hold_ns_)),
      event_queue_(std::move(other.event_queue_)) {
    other.gamepads_.clear();
    other.gamepad_back_hold_ns_.clear();
}

InputManager& InputManager::operator=(InputManager&& other) noexcept {
    if (this != &other) {
        shutdown();
        key_map_ = std::move(other.key_map_);
        gamepad_button_map_ = std::move(other.gamepad_button_map_);
        gamepads_ = std::move(other.gamepads_);
        action_states_ = std::move(other.action_states_);
        gamepad_back_hold_ns_ = std::move(other.gamepad_back_hold_ns_);
        event_queue_ = std::move(other.event_queue_);
        other.gamepads_.clear();
        other.gamepad_back_hold_ns_.clear();
    }
    return *this;
}

void InputManager::setup_default_mappings() {
    key_map_.clear();
    gamepad_button_map_.clear();

    // Keyboard defaults: Arrow keys
    key_map_[SDLK_LEFT] = GameAction::Left;
    key_map_[SDLK_DOWN] = GameAction::Down;
    key_map_[SDLK_UP] = GameAction::Up;
    key_map_[SDLK_RIGHT] = GameAction::Right;

    // Keyboard defaults: DFJK (standard 4-panel spread keys)
    key_map_[SDLK_D] = GameAction::Left;
    key_map_[SDLK_F] = GameAction::Down;
    key_map_[SDLK_J] = GameAction::Up;
    key_map_[SDLK_K] = GameAction::Right;

    // Keyboard defaults: Menu navigation
    key_map_[SDLK_RETURN] = GameAction::Confirm;
    key_map_[SDLK_KP_ENTER] = GameAction::Confirm;
    key_map_[SDLK_ESCAPE] = GameAction::Back;
    key_map_[SDLK_TAB] = GameAction::Options; // C4: open/close the options overlay

    // Gamepad defaults: D-pad
    gamepad_button_map_[SDL_GAMEPAD_BUTTON_DPAD_LEFT] = GameAction::Left;
    gamepad_button_map_[SDL_GAMEPAD_BUTTON_DPAD_DOWN] = GameAction::Down;
    gamepad_button_map_[SDL_GAMEPAD_BUTTON_DPAD_UP] = GameAction::Up;
    gamepad_button_map_[SDL_GAMEPAD_BUTTON_DPAD_RIGHT] = GameAction::Right;

    // Gamepad defaults: Face buttons for dance pads (X, A, Y, B)
    gamepad_button_map_[SDL_GAMEPAD_BUTTON_WEST] = GameAction::Left;
    gamepad_button_map_[SDL_GAMEPAD_BUTTON_SOUTH] = GameAction::Down;
    gamepad_button_map_[SDL_GAMEPAD_BUTTON_NORTH] = GameAction::Up;
    gamepad_button_map_[SDL_GAMEPAD_BUTTON_EAST] = GameAction::Right;

    // Gamepad defaults: Start / Back
    gamepad_button_map_[SDL_GAMEPAD_BUTTON_START] = GameAction::Confirm;
    gamepad_button_map_[SDL_GAMEPAD_BUTTON_BACK] = GameAction::Back;

    // Gamepad defaults: shoulders open/close the options overlay (C4). All
    // in-menu navigation reuses the directions/Confirm/Back above.
    gamepad_button_map_[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER] = GameAction::Options;
    gamepad_button_map_[SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER] = GameAction::Options;
}

void InputManager::reset_to_defaults() {
    setup_default_mappings();
}

void InputManager::bind_key(SDL_Keycode key, GameAction action) {
    key_map_[key] = action;
}

void InputManager::bind_gamepad_button(uint8_t button, GameAction action) {
    gamepad_button_map_[button] = action;
}

void InputManager::shutdown() {
    for (auto& [id, gamepad] : gamepads_) {
        if (gamepad) {
            SDL_CloseGamepad(gamepad);
        }
    }
    gamepads_.clear();
}

void InputManager::on_gamepad_added(SDL_JoystickID joystick_id) {
    SDL_Gamepad* gp = SDL_OpenGamepad(joystick_id);
    if (gp) {
        gamepads_[joystick_id] = gp;
        const char* name = SDL_GetGamepadName(gp);
        std::cout << "[InputManager] Gamepad connected: " << (name ? name : "Unknown Pad")
                  << " (ID: " << joystick_id << ")\n";
    }
}

void InputManager::on_gamepad_removed(SDL_JoystickID joystick_id) {
    auto it = gamepads_.find(joystick_id);
    if (it != gamepads_.end()) {
        std::cout << "[InputManager] Gamepad disconnected (ID: " << joystick_id << ")\n";
        if (it->second) {
            SDL_CloseGamepad(it->second);
        }
        gamepads_.erase(it);
    }
}

void InputManager::clear_action_states() {
    for (auto& entry : action_states_) {
        entry.second = false;
    }
    gamepad_back_hold_ns_.clear();
}

void InputManager::handle_gamepad_back(int device_id, bool pressed, uint64_t timestamp_ns) {
    if (pressed) {
        // Defer: a tap must stay Back, a hold becomes Options. No event is
        // emitted until release, when the gesture is disambiguated.
        gamepad_back_hold_ns_[device_id] = timestamp_ns;
        action_states_[GameAction::Back] = true;
        return;
    }

    const auto it = gamepad_back_hold_ns_.find(device_id);
    const uint64_t down_ns = it != gamepad_back_hold_ns_.end() ? it->second : timestamp_ns;
    if (it != gamepad_back_hold_ns_.end()) {
        gamepad_back_hold_ns_.erase(it);
    }
    action_states_[GameAction::Back] = false;

    const bool held = timestamp_ns >= down_ns && (timestamp_ns - down_ns) >= kBackHoldOptionsNs;
    const GameAction action = held ? GameAction::Options : GameAction::Back;

    InputEvent press_event;
    press_event.action = action;
    press_event.pressed = true;
    press_event.timestamp_ns = down_ns;
    press_event.device = DeviceType::Gamepad;
    press_event.device_id = device_id;
    press_event.raw_code = SDL_GAMEPAD_BUTTON_BACK;
    event_queue_.push_back(press_event);

    InputEvent release_event = press_event;
    release_event.pressed = false;
    release_event.timestamp_ns = timestamp_ns;
    event_queue_.push_back(release_event);
}

void InputManager::handle_sdl_event(const SDL_Event& event) {
    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        // No key-up/gamepad-up is delivered while unfocused; clear the cached
        // down-state so a hold does not stay stuck after refocus. Gameplay
        // samples is_action_down each tick, so the next update releases it.
        clear_action_states();
        return;
    }
    if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
        on_gamepad_added(event.gdevice.which);
        return;
    }
    if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
        on_gamepad_removed(event.gdevice.which);
        return;
    }

    if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
        // Skip repeat key events for gameplay trigger accuracy
        if (event.key.repeat) {
            return;
        }

        auto it = key_map_.find(event.key.key);
        if (it != key_map_.end()) {
            GameAction action = it->second;
            bool pressed = (event.type == SDL_EVENT_KEY_DOWN);
            action_states_[action] = pressed;

            InputEvent ie;
            ie.action = action;
            ie.pressed = pressed;
            ie.timestamp_ns = event.key.timestamp;
            ie.device = DeviceType::Keyboard;
            ie.device_id = 0;
            ie.raw_code = static_cast<uint32_t>(event.key.key);

            event_queue_.push_back(ie);
        }
    } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        auto it = gamepad_button_map_.find(event.gbutton.button);
        if (it != gamepad_button_map_.end()) {
            GameAction action = it->second;
            bool pressed = (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);

            if (action == GameAction::Back) {
                // Bare pads have no shoulder buttons; a hold on Back opens the
                // options overlay (see handle_gamepad_back).
                handle_gamepad_back(event.gbutton.which, pressed, event.gbutton.timestamp);
                return;
            }

            action_states_[action] = pressed;

            InputEvent ie;
            ie.action = action;
            ie.pressed = pressed;
            ie.timestamp_ns = event.gbutton.timestamp;
            ie.device = DeviceType::Gamepad;
            ie.device_id = static_cast<int>(event.gbutton.which);
            ie.raw_code = event.gbutton.button;

            event_queue_.push_back(ie);
        }
    }
}

std::vector<InputEvent> InputManager::poll_events() {
    std::vector<InputEvent> events = std::move(event_queue_);
    event_queue_.clear();
    return events;
}

bool InputManager::is_action_down(GameAction action) const {
    auto it = action_states_.find(action);
    if (it != action_states_.end()) {
        return it->second;
    }
    return false;
}

GameAction InputManager::action_for_key(SDL_Keycode key) const {
    auto it = key_map_.find(key);
    if (it != key_map_.end()) {
        return it->second;
    }
    return GameAction::None;
}

} // namespace td
