#include "input/input_manager.hpp"
#include <iostream>

#include "data/config.hpp"
#include "data/config_loader.hpp"

namespace blaze4k {

namespace {

// Config action names mirror action_to_string(); resolve a persisted action name
// back to its enum. Unknown names are ignored (never throw).
bool action_from_name(const std::string& name, GameAction& out) {
    const GameAction actions[] = {
        GameAction::Left,  GameAction::Down,    GameAction::Up,     GameAction::Right,
        GameAction::Confirm, GameAction::Back,  GameAction::Options,
    };
    for (GameAction action : actions) {
        if (action_to_string(action) == name) {
            out = action;
            return true;
        }
    }
    return false;
}

SDL_Keycode key_from_name(const std::string& name, bool& ok) {
    const SDL_Keycode key = SDL_GetKeyFromName(name.c_str());
    ok = key != SDLK_UNKNOWN;
    return key;
}

SDL_GamepadButton button_from_name(const std::string& name, bool& ok) {
    const SDL_GamepadButton button = SDL_GetGamepadButtonFromString(name.c_str());
    ok = button != SDL_GAMEPAD_BUTTON_INVALID;
    return button;
}

// Merge `bindings` (persisted per-action overrides) over `defaults` for one
// device. Iterating the compiled defaults is what makes a missing action fall
// back: an action absent from `bindings`, or present with no parseable name,
// keeps its compiled default bindings. input_remap_from_config() uses the same
// absent/empty-override fallback, so the screen and the runtime agree for every
// config written through the UI. They differ only for a hand-edited override
// whose names all fail to parse: the screen lists those names, the runtime
// ignores them and keeps the defaults (see the C6 remap screen).
template <typename Code, typename FromName>
void apply_bindings_impl(std::unordered_map<Code, GameAction>& map,
                         const std::vector<InputBinding>& bindings,
                         const std::vector<InputBinding>& defaults, const char* device_label,
                         FromName from_name) {
    const auto is_default_action = [&defaults](const std::string& name) {
        for (const InputBinding& def : defaults) {
            if (def.first == name) {
                return true;
            }
        }
        return false;
    };
    // A persisted action that matches no compiled default action is unknown and
    // ignored (logged once), never bound.
    for (const InputBinding& binding : bindings) {
        if (!is_default_action(binding.first)) {
            std::cout << "[InputManager] unknown input action '" << binding.first << "'\n";
        }
    }

    for (const InputBinding& def : defaults) {
        GameAction action = GameAction::None;
        if (!action_from_name(def.first, action)) {
            continue;
        }

        const InputBinding* chosen = &def;
        for (const InputBinding& binding : bindings) {
            if (binding.first == def.first) {
                chosen = &binding;
                break;
            }
        }

        bool any_valid = false;
        for (const std::string& name : chosen->second) {
            bool ok = false;
            const Code code = from_name(name, ok);
            if (!ok) {
                std::cout << "[InputManager] unknown " << device_label << " binding name '" << name
                          << "' for action '" << def.first << "'\n";
                continue;
            }
            map[code] = action;
            any_valid = true;
        }
        if (any_valid) {
            continue;
        }

        // Absent override, or every name failed to parse: keep the defaults.
        for (const std::string& name : def.second) {
            bool ok = false;
            const Code code = from_name(name, ok);
            if (ok) {
                map[code] = action;
            }
        }
    }
}

} // namespace

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
      event_queue_(std::move(other.event_queue_)),
      capture_mode_(other.capture_mode_) {
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
        capture_mode_ = other.capture_mode_;
        other.gamepads_.clear();
        other.gamepad_back_hold_ns_.clear();
    }
    return *this;
}

void InputManager::apply_bindings(const InputSettings& settings) {
    key_map_.clear();
    gamepad_button_map_.clear();

    apply_bindings_impl(key_map_, settings.key_bindings, default_key_bindings(), "key",
                        key_from_name);
    apply_bindings_impl(gamepad_button_map_, settings.gamepad_bindings,
                        default_gamepad_bindings(), "gamepad", button_from_name);

    // Reserved safety bindings: no remap can ever remove the escape hatch, so a
    // bad binding can never soft-lock the shell (see the C6 screen).
    key_map_[SDLK_ESCAPE] = GameAction::Back;
    gamepad_button_map_[SDL_GAMEPAD_BUTTON_BACK] = GameAction::Back;
}

void InputManager::setup_default_mappings() {
    apply_bindings(InputSettings{});
}

void InputManager::reset_to_defaults() {
    apply_bindings(InputSettings{});
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

        if (capture_mode_) {
            InputEvent ie;
            ie.action = GameAction::None;
            ie.pressed = (event.type == SDL_EVENT_KEY_DOWN);
            ie.timestamp_ns = event.key.timestamp;
            ie.device = DeviceType::Keyboard;
            ie.device_id = 0;
            ie.raw_code = static_cast<uint32_t>(event.key.key);
            event_queue_.push_back(ie);
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
        if (capture_mode_) {
            InputEvent ie;
            ie.action = GameAction::None;
            ie.pressed = (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
            ie.timestamp_ns = event.gbutton.timestamp;
            ie.device = DeviceType::Gamepad;
            ie.device_id = static_cast<int>(event.gbutton.which);
            ie.raw_code = event.gbutton.button;
            event_queue_.push_back(ie);
            return;
        }

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

} // namespace blaze4k
