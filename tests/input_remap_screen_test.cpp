#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "data/config.hpp"
#include "input/input_manager.hpp"
#include "render/gl_quad_renderer.hpp"
#include "screens/input_remap_screen.hpp"
#include "screens/screen.hpp"
#include "screens/screen_manager.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

using td::DeviceType;
using td::GameAction;
using td::InputEvent;
using td::ScreenContext;
using td::ScreenId;

class SelectSpy : public td::Screen {
public:
    explicit SelectSpy(int* enters) : enters_(enters) {}
    [[nodiscard]] ScreenId id() const override { return ScreenId::Select; }
    void enter(ScreenContext& /*ctx*/) override {
        if (enters_ != nullptr) {
            ++(*enters_);
        }
    }

private:
    int* enters_ = nullptr;
};

InputEvent press(GameAction action) {
    InputEvent event;
    event.action = action;
    event.pressed = true;
    return event;
}

InputEvent raw_press(DeviceType device, uint32_t code, uint64_t ts_ns = 42) {
    InputEvent event;
    event.action = GameAction::None;
    event.pressed = true;
    event.timestamp_ns = ts_ns;
    event.device = device;
    event.raw_code = code;
    return event;
}

bool config_has(const td::GameConfig& config, const std::string& action, const std::string& name) {
    for (const td::InputBinding& binding : config.input.key_bindings) {
        if (binding.first != action) {
            continue;
        }
        for (const std::string& value : binding.second) {
            if (value == name) {
                return true;
            }
        }
    }
    return false;
}

struct Fixture {
    td::InputManager input;
    td::ScreenManager manager{0.0};
    td::InputRemapScreen* screen = nullptr;
    td::GameConfig config;
    int select_enters = 0;

    Fixture() {
        auto owner = std::make_unique<td::InputRemapScreen>();
        screen = owner.get();
        manager.add_screen(std::move(owner));
        manager.add_screen(std::make_unique<SelectSpy>(&select_enters));
        manager.context().config = &config;
        manager.context().input = &input;
        manager.start(ScreenId::InputRemap);
    }

    ScreenContext& ctx() { return manager.context(); }
};

void test_rows_and_exit() {
    Fixture f;
    TEST_CHECK(!f.screen->model().rows.empty());
    TEST_CHECK(!f.screen->capturing());

    // exit clears any capture mode.
    f.screen->update(f.ctx(), 0.0, {press(GameAction::Confirm)});
    TEST_CHECK(f.screen->capturing());
    TEST_CHECK(f.input.capture_mode());
    f.screen->exit(f.ctx());
    TEST_CHECK(!f.input.capture_mode());
    std::cout << "  - 1. rows from config + exit clears capture ok.\n";
}

void test_capture_start() {
    Fixture f;
    f.screen->update(f.ctx(), 0.0, {press(GameAction::Confirm)});
    TEST_CHECK(f.screen->capturing());
    TEST_CHECK(f.input.capture_mode());
    std::cout << "  - 2. Confirm starts capture ok.\n";
}

void test_capture_assign() {
    Fixture f;
    const auto& rows = f.screen->model().rows;
    TEST_CHECK(!rows.empty() && rows[0].action == GameAction::Left);

    f.screen->update(f.ctx(), 0.0, {press(GameAction::Confirm)});
    f.screen->update(f.ctx(), 0.0, {raw_press(DeviceType::Keyboard, SDLK_SPACE, 777)});

    TEST_CHECK(!f.screen->capturing());
    TEST_CHECK(!f.input.capture_mode());
    TEST_CHECK(f.input.action_for_key(SDLK_SPACE) == GameAction::Left);
    TEST_CHECK(config_has(f.config, "Left", "Space"));
    std::cout << "  - 3. capture -> name -> immediate apply ok.\n";
}

void test_conflict() {
    Fixture f;
    f.screen->update(f.ctx(), 0.0, {press(GameAction::Confirm)});
    // "Down" already belongs to the Down action.
    f.screen->update(f.ctx(), 0.0, {raw_press(DeviceType::Keyboard, SDLK_DOWN)});

    TEST_CHECK(!f.screen->model().message.empty());
    TEST_CHECK(!config_has(f.config, "Left", "Down"));
    TEST_CHECK(f.screen->model().rows[0].name == "Left"); // unchanged
    TEST_CHECK(f.screen->capturing());                    // still capturing after rejection
    std::cout << "  - 4. conflict rejected with a message ok.\n";
}

void test_escape_cancels() {
    Fixture f;
    f.screen->update(f.ctx(), 0.0, {press(GameAction::Confirm)});
    f.screen->update(f.ctx(), 0.0, {raw_press(DeviceType::Keyboard, SDLK_ESCAPE)});

    TEST_CHECK(!f.screen->capturing());
    TEST_CHECK(!f.input.capture_mode());
    TEST_CHECK(f.screen->model().rows[0].name == "Left");
    std::cout << "  - 5. Escape cancels capture ok.\n";
}

void test_back_exits() {
    Fixture f;
    TEST_CHECK(f.manager.back_navigates());
    TEST_CHECK(f.screen->handle_back(f.ctx()) == false); // not capturing
    f.manager.update(0.0, {press(GameAction::Back)});
    TEST_CHECK(f.manager.active_id() == ScreenId::Select);
    std::cout << "  - 6. Back exits to Select ok.\n";
}

void test_reset() {
    Fixture f;
    const int downs = static_cast<int>(f.screen->model().rows.size());
    for (int i = 0; i < downs; ++i) {
        f.screen->update(f.ctx(), 0.0, {press(GameAction::Down)});
    }
    TEST_CHECK(f.screen->reset_selected());
    f.screen->update(f.ctx(), 0.0, {press(GameAction::Confirm)});

    TEST_CHECK(config_has(f.config, "Left", "D"));
    TEST_CHECK(config_has(f.config, "Options", "Tab"));
    TEST_CHECK(f.input.action_for_key(SDLK_TAB) == GameAction::Options);
    TEST_CHECK(f.input.action_for_key(SDLK_ESCAPE) == GameAction::Back);
    TEST_CHECK(f.input.action_for_key(SDLK_LEFT) == GameAction::Left);
    std::cout << "  - 7. reset restores defaults + live output ok.\n";
}

void test_reserved_safety() {
    Fixture f;
    td::InputSettings settings;
    settings.key_bindings = {{"Left", {"A"}}}; // omits Escape entirely
    f.input.apply_bindings(settings);
    TEST_CHECK(f.input.action_for_key(SDLK_ESCAPE) == GameAction::Back);
    std::cout << "  - 8. reserved Escape -> Back ok.\n";
}

void test_render_and_reenter() {
    Fixture f;
    td::GlQuadRenderer renderer; // uninitialized: draws are no-ops
    f.screen->render(f.ctx(), renderer, 1280, 720);
    f.screen->render(f.ctx(), renderer, 0, 0);

    f.screen->update(f.ctx(), 0.0, {press(GameAction::Confirm)});
    TEST_CHECK(f.screen->capturing());
    f.manager.start(ScreenId::InputRemap); // exit + re-enter
    TEST_CHECK(!f.screen->capturing());
    TEST_CHECK(!f.screen->reset_selected());
    std::cout << "  - 9. headless render + re-enter resets state ok.\n";
}

} // namespace

int main() {
    std::cout << "[input_remap_screen_test] Running headless remap-screen tests...\n";
    test_rows_and_exit();
    test_capture_start();
    test_capture_assign();
    test_conflict();
    test_escape_cancels();
    test_back_exits();
    test_reset();
    test_reserved_safety();
    test_render_and_reenter();
    std::cout << "[input_remap_screen_test] All tests passed!\n";
    return 0;
}
