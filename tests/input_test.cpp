#include <iostream>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <cassert>
#include "input/input_manager.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr << "\n"; \
            std::abort(); \
        } \
    } while (0)

int main() {
    std::cout << "[input_test] Starting input layer unit tests...\n";

    td::InputManager input;

    // 1. Test Keyboard Mapping & Nanosecond Timestamps
    const uint64_t fake_timestamp_1 = 1234567890123ULL;
    SDL_Event key_down{};
    key_down.type = SDL_EVENT_KEY_DOWN;
    key_down.key.key = SDLK_LEFT;
    key_down.key.down = true;
    key_down.key.repeat = false;
    key_down.key.timestamp = fake_timestamp_1;

    input.handle_sdl_event(key_down);
    TEST_CHECK(input.is_action_down(td::GameAction::Left));

    std::vector<td::InputEvent> events = input.poll_events();
    TEST_CHECK(events.size() == 1);
    TEST_CHECK(events[0].action == td::GameAction::Left);
    TEST_CHECK(events[0].pressed == true);
    TEST_CHECK(events[0].timestamp_ns == fake_timestamp_1);
    TEST_CHECK(events[0].device == td::DeviceType::Keyboard);
    TEST_CHECK(events[0].raw_code == SDLK_LEFT);
    std::cout << "  - Keyboard event mapped and nanosecond timestamp verified.\n";

    // After poll, queue is cleared
    TEST_CHECK(input.poll_events().empty());

    // 2. Test Key Release
    const uint64_t fake_timestamp_2 = 1234567990123ULL;
    SDL_Event key_up{};
    key_up.type = SDL_EVENT_KEY_UP;
    key_up.key.key = SDLK_LEFT;
    key_up.key.down = false;
    key_up.key.repeat = false;
    key_up.key.timestamp = fake_timestamp_2;

    input.handle_sdl_event(key_up);
    TEST_CHECK(!input.is_action_down(td::GameAction::Left));

    events = input.poll_events();
    TEST_CHECK(events.size() == 1);
    TEST_CHECK(events[0].action == td::GameAction::Left);
    TEST_CHECK(events[0].pressed == false);
    TEST_CHECK(events[0].timestamp_ns == fake_timestamp_2);
    std::cout << "  - Key release updates state and timestamps.\n";

    // 3. Test 4-Panel Spread Keys (D, F, J, K)
    SDL_Event dfjk_events[4]{};
    SDL_Keycode dfjk_keys[4] = {SDLK_D, SDLK_F, SDLK_J, SDLK_K};
    td::GameAction expected_actions[4] = {
        td::GameAction::Left, td::GameAction::Down,
        td::GameAction::Up, td::GameAction::Right
    };

    for (int i = 0; i < 4; ++i) {
        dfjk_events[i].type = SDL_EVENT_KEY_DOWN;
        dfjk_events[i].key.key = dfjk_keys[i];
        dfjk_events[i].key.down = true;
        dfjk_events[i].key.timestamp = 2000000000ULL + i * 1000ULL;
        input.handle_sdl_event(dfjk_events[i]);
    }

    events = input.poll_events();
    TEST_CHECK(events.size() == 4);
    for (int i = 0; i < 4; ++i) {
        TEST_CHECK(events[i].action == expected_actions[i]);
        TEST_CHECK(events[i].timestamp_ns == 2000000000ULL + i * 1000ULL);
        TEST_CHECK(input.is_action_down(expected_actions[i]));
    }
    std::cout << "  - 4-panel spread keys (DFJK) mapped successfully.\n";

    // 3b. C4 options action: Tab (keyboard) and shoulder buttons (gamepad)
    TEST_CHECK(input.action_for_key(SDLK_TAB) == td::GameAction::Options);

    SDL_Event tab_down{};
    tab_down.type = SDL_EVENT_KEY_DOWN;
    tab_down.key.key = SDLK_TAB;
    tab_down.key.down = true;
    tab_down.key.repeat = false;
    tab_down.key.timestamp = 2500000000ULL;
    input.handle_sdl_event(tab_down);

    events = input.poll_events();
    TEST_CHECK(events.size() == 1);
    TEST_CHECK(events[0].action == td::GameAction::Options);
    TEST_CHECK(events[0].device == td::DeviceType::Keyboard);

    SDL_Event shoulder_down{};
    shoulder_down.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    shoulder_down.gbutton.button = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    shoulder_down.gbutton.down = true;
    shoulder_down.gbutton.which = 1;
    shoulder_down.gbutton.timestamp = 2500001000ULL;
    input.handle_sdl_event(shoulder_down);

    events = input.poll_events();
    TEST_CHECK(events.size() == 1);
    TEST_CHECK(events[0].action == td::GameAction::Options);
    TEST_CHECK(events[0].device == td::DeviceType::Gamepad);

    SDL_Event right_shoulder_down{};
    right_shoulder_down.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    right_shoulder_down.gbutton.button = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    right_shoulder_down.gbutton.down = true;
    right_shoulder_down.gbutton.which = 1;
    right_shoulder_down.gbutton.timestamp = 2500002000ULL;
    input.handle_sdl_event(right_shoulder_down);

    events = input.poll_events();
    TEST_CHECK(events.size() == 1);
    TEST_CHECK(events[0].action == td::GameAction::Options);
    std::cout << "  - Options action mapped to Tab + shoulder buttons.\n";

    // 3c. Hold-Back fallback for bare pads: a short tap stays Back, a hold
    // synthesizes Options so the overlay is reachable without shoulders.
    auto send_pad_back = [&input](bool pressed, uint64_t ts) {
        SDL_Event e{};
        e.type = pressed ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
        e.gbutton.button = SDL_GAMEPAD_BUTTON_BACK;
        e.gbutton.down = pressed;
        e.gbutton.which = 1;
        e.gbutton.timestamp = ts;
        input.handle_sdl_event(e);
    };

    const uint64_t tap_down_ns = 2600000000ULL;
    send_pad_back(true, tap_down_ns);
    TEST_CHECK(input.poll_events().empty()); // deferred until release
    send_pad_back(false, tap_down_ns + 100000000ULL); // 100 ms tap
    events = input.poll_events();
    TEST_CHECK(events.size() == 2);
    TEST_CHECK(events[0].action == td::GameAction::Back && events[0].pressed);
    TEST_CHECK(events[1].action == td::GameAction::Back && !events[1].pressed);

    const uint64_t hold_down_ns = 2700000000ULL;
    send_pad_back(true, hold_down_ns);
    TEST_CHECK(input.poll_events().empty());
    send_pad_back(false, hold_down_ns + td::InputManager::kBackHoldOptionsNs + 100000000ULL);
    events = input.poll_events();
    TEST_CHECK(events.size() == 2);
    TEST_CHECK(events[0].action == td::GameAction::Options && events[0].pressed);
    TEST_CHECK(events[1].action == td::GameAction::Options && !events[1].pressed);
    std::cout << "  - Hold-Back opens Options on shoulder-less pads.\n";

    // 4. Test Gamepad / Dance Pad Button Mapping
    const uint64_t pad_timestamp = 3456789012345ULL;
    SDL_Event pad_down{};
    pad_down.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    pad_down.gbutton.button = SDL_GAMEPAD_BUTTON_DPAD_UP;
    pad_down.gbutton.down = true;
    pad_down.gbutton.which = 1;
    pad_down.gbutton.timestamp = pad_timestamp;

    input.handle_sdl_event(pad_down);
    TEST_CHECK(input.is_action_down(td::GameAction::Up));

    events = input.poll_events();
    TEST_CHECK(events.size() == 1);
    TEST_CHECK(events[0].action == td::GameAction::Up);
    TEST_CHECK(events[0].device == td::DeviceType::Gamepad);
    TEST_CHECK(events[0].device_id == 1);
    TEST_CHECK(events[0].timestamp_ns == pad_timestamp);
    std::cout << "  - Gamepad / dance pad button mapped with exact timestamp.\n";

    // 5. Test Hotplug Events Handling (disconnecting joystick ID 1)
    SDL_Event disconnect{};
    disconnect.type = SDL_EVENT_GAMEPAD_REMOVED;
    disconnect.gdevice.which = 1;
    input.handle_sdl_event(disconnect);
    std::cout << "  - Gamepad disconnect handled gracefully.\n";

    // 6. Test Custom Key Rebinding
    input.bind_key(SDLK_SPACE, td::GameAction::Confirm);
    SDL_Event space_down{};
    space_down.type = SDL_EVENT_KEY_DOWN;
    space_down.key.key = SDLK_SPACE;
    space_down.key.down = true;
    space_down.key.timestamp = 4000000000ULL;
    input.handle_sdl_event(space_down);

    events = input.poll_events();
    TEST_CHECK(events.size() == 1);
    TEST_CHECK(events[0].action == td::GameAction::Confirm);
    std::cout << "  - Custom key rebinding verified.\n";

    // 7. Focus loss clears the cached down-state so a missed release cannot
    // leave a held note stuck.
    input.handle_sdl_event(dfjk_events[0]); // Left is held again
    TEST_CHECK(input.is_action_down(td::GameAction::Left));
    SDL_Event focus_lost{};
    focus_lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    input.handle_sdl_event(focus_lost);
    TEST_CHECK(!input.is_action_down(td::GameAction::Left));
    TEST_CHECK(!input.is_action_down(td::GameAction::Down));
    TEST_CHECK(!input.is_action_down(td::GameAction::Up));
    TEST_CHECK(!input.is_action_down(td::GameAction::Right));
    std::cout << "  - Focus loss clears held-action state.\n";

    std::cout << "[input_test] All input tests passed successfully!\n";
    return 0;
}
