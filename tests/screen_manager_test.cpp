#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "app/app.hpp"
#include "render/bitmap_font.hpp"
#include "render/gl_quad_renderer.hpp"
#include "screens/attract_screen.hpp"
#include "screens/screen.hpp"
#include "screens/screen_manager.hpp"
#include "screens/select_placeholder_screen.hpp"
#include "screens/title_screen.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

using td::GameAction;
using td::InputEvent;
using td::ScreenContext;
using td::ScreenId;

std::vector<std::string> g_log;

std::string name_of(ScreenId id) { return std::string(td::screen_id_name(id)); }

InputEvent press(GameAction action) {
    InputEvent event;
    event.action = action;
    event.pressed = true;
    return event;
}

class SpyScreen : public td::Screen {
public:
    explicit SpyScreen(ScreenId screen_id) : sid_(screen_id) {}

    [[nodiscard]] ScreenId id() const override { return sid_; }

    void enter(ScreenContext& /*ctx*/) override {
        ++enter_count;
        g_log.push_back("enter:" + name_of(sid_));
    }
    void update(ScreenContext& ctx, double /*fixed_dt*/,
                const std::vector<InputEvent>& /*events*/) override {
        ++update_count;
        active_was_source = ctx.manager != nullptr && ctx.manager->active_id() == sid_;
        if (transition_on_update && ctx.manager != nullptr) {
            ctx.manager->transition_to(transition_target);
        }
    }
    void render(ScreenContext& /*ctx*/, td::GlQuadRenderer& /*renderer*/, int /*w*/,
                int /*h*/) override {
        ++render_count;
    }
    void exit(ScreenContext& /*ctx*/) override {
        ++exit_count;
        g_log.push_back("exit:" + name_of(sid_));
    }
    void update_inactive(double /*fixed_dt*/) override { ++inactive_update_count; }

    int enter_count = 0;
    int update_count = 0;
    int render_count = 0;
    int exit_count = 0;
    int inactive_update_count = 0;
    bool transition_on_update = false;
    ScreenId transition_target = ScreenId::Title;
    bool active_was_source = false;

private:
    ScreenId sid_;
};

struct SpyRef {
    SpyScreen* ptr = nullptr;
};

SpyRef add_spy(td::ScreenManager& manager, ScreenId id) {
    auto owner = std::make_unique<SpyScreen>(id);
    SpyScreen* raw = owner.get();
    manager.add_screen(std::move(owner));
    return SpyRef{raw};
}

constexpr double kDt = 0.1;

void test_boot_lifecycle() {
    td::ScreenManager manager;
    SpyRef title = add_spy(manager, ScreenId::Title);

    manager.start(ScreenId::Title);
    TEST_CHECK(manager.active_id() == ScreenId::Title);
    TEST_CHECK(title.ptr->enter_count == 1);
    TEST_CHECK(title.ptr->exit_count == 0);

    // A repeated start() must exit the outgoing screen (enter/exit pairing).
    manager.start(ScreenId::Title);
    TEST_CHECK(title.ptr->exit_count == 1);
    TEST_CHECK(title.ptr->enter_count == 2);
    std::cout << "  - boot lifecycle ok.\n";
}

void test_transition_ordering() {
    td::ScreenManager manager;
    SpyRef title = add_spy(manager, ScreenId::Title);
    SpyRef select = add_spy(manager, ScreenId::Select);
    manager.start(ScreenId::Title);

    g_log.clear();
    manager.transition_to(ScreenId::Select);
    manager.update(kDt, {});
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    TEST_CHECK(title.ptr->exit_count == 1);
    TEST_CHECK(select.ptr->enter_count == 1);
    TEST_CHECK(g_log.size() == 2);
    TEST_CHECK(g_log[0] == "exit:Title");
    TEST_CHECK(g_log[1] == "enter:Select");
    std::cout << "  - transition ordering (exit before enter) ok.\n";
}

void test_deferred_application() {
    td::ScreenManager manager;
    SpyRef title = add_spy(manager, ScreenId::Title);
    SpyRef select = add_spy(manager, ScreenId::Select);
    title.ptr->transition_on_update = true;
    title.ptr->transition_target = ScreenId::Select;
    manager.start(ScreenId::Title);

    manager.update(kDt, {});
    TEST_CHECK(title.ptr->update_count == 1);
    TEST_CHECK(title.ptr->active_was_source); // not yet exited during its own update
    TEST_CHECK(title.ptr->exit_count == 1);    // applied only after update returned
    TEST_CHECK(select.ptr->enter_count == 1);
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    std::cout << "  - deferred application (no self-destroy) ok.\n";
}

void test_inactive_screens_ticked() {
    td::ScreenManager manager;
    SpyRef title = add_spy(manager, ScreenId::Title);
    SpyRef select = add_spy(manager, ScreenId::Select);
    manager.start(ScreenId::Title);

    manager.update(kDt, {});
    TEST_CHECK(title.ptr->update_count == 1);
    TEST_CHECK(title.ptr->inactive_update_count == 0); // active: update() only
    TEST_CHECK(select.ptr->inactive_update_count == 1);

    manager.transition_to(ScreenId::Select);
    manager.update(kDt, {});
    TEST_CHECK(title.ptr->inactive_update_count == 1);
    TEST_CHECK(select.ptr->inactive_update_count == 1);
    std::cout << "  - inactive screens get update_inactive() each tick ok.\n";
}

void test_unregistered_target() {
    td::ScreenManager manager;
    SpyRef title = add_spy(manager, ScreenId::Title);
    manager.start(ScreenId::Title);

    manager.transition_to(ScreenId::Gameplay); // nothing registered
    manager.update(kDt, {});
    TEST_CHECK(manager.active_id() == ScreenId::Title);
    TEST_CHECK(title.ptr->exit_count == 0);
    TEST_CHECK(!manager.has_screen(ScreenId::Gameplay));
    std::cout << "  - unregistered target is a safe no-op.\n";
}

void test_idle_attract_policy() {
    // 0.125 is exactly representable in binary, so 8 steps sum to exactly 1.0
    // without float-accumulation drift at the timeout boundary.
    constexpr double idle_dt = 0.125;

    td::ScreenManager manager(1.0);
    SpyRef title = add_spy(manager, ScreenId::Title);
    SpyRef attract = add_spy(manager, ScreenId::Attract);
    manager.start(ScreenId::Title);

    for (int i = 0; i < 7; ++i) {
        manager.update(idle_dt, {});
    }
    TEST_CHECK(manager.active_id() == ScreenId::Title);
    manager.update(idle_dt, {});
    TEST_CHECK(manager.active_id() == ScreenId::Attract);
    TEST_CHECK(manager.attract_return() == ScreenId::Title);
    TEST_CHECK(title.ptr->exit_count == 1);
    TEST_CHECK(attract.ptr->enter_count == 1);

    // A press before the timeout resets the accumulator.
    td::ScreenManager reset_manager(1.0);
    add_spy(reset_manager, ScreenId::Title);
    add_spy(reset_manager, ScreenId::Attract);
    reset_manager.start(ScreenId::Title);
    for (int i = 0; i < 5; ++i) {
        reset_manager.update(idle_dt, {});
    }
    reset_manager.update(idle_dt, {press(GameAction::Confirm)});
    TEST_CHECK(reset_manager.idle_seconds() == 0.0);
    TEST_CHECK(reset_manager.active_id() == ScreenId::Title);
    for (int i = 0; i < 7; ++i) {
        reset_manager.update(idle_dt, {});
    }
    TEST_CHECK(reset_manager.active_id() == ScreenId::Title); // 0.875 s < 1.0 s
    std::cout << "  - idle-attract policy + accumulator reset ok.\n";
}

void test_idle_only_from_title_select() {
    td::ScreenManager manager(1.0);
    SpyRef gameplay = add_spy(manager, ScreenId::Gameplay);
    manager.start(ScreenId::Gameplay);
    for (int i = 0; i < 20; ++i) {
        manager.update(kDt, {});
    }
    TEST_CHECK(manager.active_id() == ScreenId::Gameplay);
    TEST_CHECK(gameplay.ptr->exit_count == 0);
    std::cout << "  - idle does not trigger from Gameplay ok.\n";
}

void test_idle_disable() {
    // A zero timeout via the setter disables idle -> Attract.
    td::ScreenManager manager;
    add_spy(manager, ScreenId::Title);
    add_spy(manager, ScreenId::Attract);
    manager.set_idle_timeout_seconds(0.0);
    manager.start(ScreenId::Title);
    for (int i = 0; i < 20; ++i) {
        manager.update(kDt, {});
    }
    TEST_CHECK(manager.active_id() == ScreenId::Title);

    // A negative timeout (constructor or CLI) also disables it.
    td::ScreenManager negative(-5.0);
    add_spy(negative, ScreenId::Title);
    add_spy(negative, ScreenId::Attract);
    negative.start(ScreenId::Title);
    for (int i = 0; i < 20; ++i) {
        negative.update(kDt, {});
    }
    TEST_CHECK(negative.active_id() == ScreenId::Title);
    std::cout << "  - idle disable for timeout <= 0 ok.\n";
}

void test_attract_return() {
    // From Title.
    {
        td::ScreenManager manager(1.0);
        add_spy(manager, ScreenId::Title);
        add_spy(manager, ScreenId::Attract);
        manager.start(ScreenId::Title);
        manager.update(2.0, {});
        TEST_CHECK(manager.active_id() == ScreenId::Attract);
        manager.update(kDt, {press(GameAction::Confirm)});
        TEST_CHECK(manager.active_id() == ScreenId::Title);
    }
    // From Select.
    {
        td::ScreenManager manager(1.0);
        add_spy(manager, ScreenId::Title);
        add_spy(manager, ScreenId::Select);
        add_spy(manager, ScreenId::Attract);
        manager.start(ScreenId::Select);
        manager.update(2.0, {});
        TEST_CHECK(manager.active_id() == ScreenId::Attract);
        TEST_CHECK(manager.attract_return() == ScreenId::Select);
        manager.update(kDt, {press(GameAction::Confirm)});
        TEST_CHECK(manager.active_id() == ScreenId::Select);
    }
    std::cout << "  - attract returns to origin ok.\n";
}

void test_back_navigation() {
    td::ScreenManager manager(1.0);
    add_spy(manager, ScreenId::Title);
    add_spy(manager, ScreenId::Select);
    add_spy(manager, ScreenId::Attract);

    manager.start(ScreenId::Title);
    TEST_CHECK(!manager.back_navigates());
    manager.start(ScreenId::Select);
    TEST_CHECK(manager.back_navigates());
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(manager.active_id() == ScreenId::Title);

    manager.start(ScreenId::Select);
    manager.update(2.0, {}); // idle -> Attract
    TEST_CHECK(manager.active_id() == ScreenId::Attract);
    TEST_CHECK(manager.back_navigates());
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    std::cout << "  - back navigation ok.\n";
}

void test_calibration_back_navigation() {
    td::ScreenManager manager(0.0);
    add_spy(manager, ScreenId::Title);
    add_spy(manager, ScreenId::Calibration);
    add_spy(manager, ScreenId::Select);

    manager.start(ScreenId::Calibration);
    TEST_CHECK(manager.back_navigates()); // Calibration is in the default set
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    std::cout << "  - Calibration Back -> Select (abort) ok.\n";
}

void test_input_remap_back_navigation() {
    td::ScreenManager manager(0.0);
    add_spy(manager, ScreenId::Title);
    add_spy(manager, ScreenId::InputRemap);
    add_spy(manager, ScreenId::Select);

    manager.start(ScreenId::InputRemap);
    TEST_CHECK(manager.back_navigates()); // InputRemap is in the default set
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    std::cout << "  - InputRemap Back -> Select ok.\n";
}

void test_results_back_navigation() {
    td::ScreenManager manager(0.0);
    add_spy(manager, ScreenId::Title);
    add_spy(manager, ScreenId::Results);
    add_spy(manager, ScreenId::Select);

    manager.start(ScreenId::Results);
    TEST_CHECK(manager.back_navigates()); // Results is in the default set
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    std::cout << "  - Results Back -> Select ok.\n";
}

void test_render_dispatch_headless() {
    td::GlQuadRenderer renderer; // uninitialized: draws are no-ops
    td::ScreenManager manager;
    SpyRef title = add_spy(manager, ScreenId::Title);
    SpyRef select = add_spy(manager, ScreenId::Select);
    manager.start(ScreenId::Title);

    manager.render(renderer, 1280, 720);
    TEST_CHECK(title.ptr->render_count == 1);
    TEST_CHECK(select.ptr->render_count == 0);
    manager.transition_to(ScreenId::Select);
    manager.update(kDt, {});
    manager.render(renderer, 1280, 720);
    TEST_CHECK(select.ptr->render_count == 1);
    std::cout << "  - headless render dispatch ok.\n";
}

void test_real_screens() {
    td::GlQuadRenderer renderer;
    td::ScreenManager manager(0.5);
    manager.add_screen(std::make_unique<td::TitleScreen>());
    manager.add_screen(std::make_unique<td::AttractScreen>());
    manager.add_screen(std::make_unique<td::SelectPlaceholderScreen>());
    manager.start(ScreenId::Title);

    // Title + Confirm -> Select.
    manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);

    // Select + Back -> Title.
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(manager.active_id() == ScreenId::Title);

    // Idle from Title -> Attract.
    manager.update(1.0, {});
    TEST_CHECK(manager.active_id() == ScreenId::Attract);
    manager.render(renderer, 1280, 720);

    // Attract + Confirm -> origin (Title).
    manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(manager.active_id() == ScreenId::Title);
    manager.render(renderer, 1280, 720);
    std::cout << "  - real Title/Attract/Select screens ok.\n";
}

void test_font_sanity() {
    td::GlQuadRenderer renderer; // uninitialized no-op
    TEST_CHECK(td::text_width("PRESS START", 3.0f) > 0.0f);
    TEST_CHECK(td::text_width("", 3.0f) == 0.0f);
    // Monospace cell arithmetic: each glyph advances 6*pixel.
    TEST_CHECK(td::text_width("A", 1.0f) == 6.0f);
    TEST_CHECK(td::text_width("AB", 2.0f) == 24.0f);

    const std::string text = "TUNDRA DANCE";
    const float pixel = 4.0f;
    // Pixel-accurate centering is not observable through the no-op renderer seam;
    // exercise the call and verify the width it centers against.
    TEST_CHECK(td::text_width(text, pixel) == static_cast<float>(text.size()) * 6.0f * pixel);
    td::draw_text(renderer, "PRESS START", 0.0f, 0.0f, 3.0f, td::Color{});
    td::draw_text_centered(renderer, text, 640.0f, 0.0f, pixel, td::Color{});
    std::cout << "  - bitmap font sanity ok.\n";
}

void test_app_shell_smoke() {
    td::AppConfig cfg;
    cfg.window.headless = true;
    cfg.smoke_test_frames = 30;

    td::ScreenManager manager;
    manager.add_screen(std::make_unique<td::TitleScreen>());
    manager.add_screen(std::make_unique<td::AttractScreen>());
    manager.add_screen(std::make_unique<td::SelectPlaceholderScreen>());
    manager.start(ScreenId::Title);

    td::GlQuadRenderer renderer;
    td::App app(cfg);
    app.set_update_callback([&](double fixed_dt) {
        manager.update(fixed_dt, app.input_manager().poll_events());
    });
    app.set_render_callback([&](double) {
        manager.render(renderer, app.window().width(), app.window().height());
    });

    TEST_CHECK(app.init());
    app.run();
    TEST_CHECK(!app.is_running());
    TEST_CHECK(manager.active_id() == ScreenId::Title);
    std::cout << "  - App + shell headless smoke ok.\n";
}

// AC3 (logic level): the arcade loop must have no dead ends. Every canonical
// screen is registered and leaves on a documented edge; the forward chain
// Title -> Select -> Gameplay -> Results -> Select -> Title traverses cleanly.
// Real Title/Attract/SelectPlaceholder screens supply the Title->Select edge;
// Gameplay/Results are spies so the test needs no audio device or play request.
void test_arcade_loop_no_dead_ends() {
    td::ScreenManager manager(0.0);
    manager.add_screen(std::make_unique<td::TitleScreen>());
    manager.add_screen(std::make_unique<td::AttractScreen>());
    manager.add_screen(std::make_unique<td::SelectPlaceholderScreen>());
    SpyRef gameplay = add_spy(manager, ScreenId::Gameplay);
    SpyRef results = add_spy(manager, ScreenId::Results);

    const ScreenId canonical[] = {ScreenId::Title, ScreenId::Attract, ScreenId::Select,
                                  ScreenId::Gameplay, ScreenId::Results};
    for (ScreenId id : canonical) {
        TEST_CHECK(manager.has_screen(id));
    }

    // Title --Confirm--> Select (real TitleScreen edge).
    manager.start(ScreenId::Title);
    manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);

    // Select --(Confirm handoff)--> Gameplay. The real SelectScreen publishes the
    // request edge; the placeholder has no library here, so drive the documented
    // transition and prove the manager lands on the real Gameplay id.
    manager.transition_to(ScreenId::Gameplay);
    manager.update(kDt, {});
    TEST_CHECK(manager.active_id() == ScreenId::Gameplay);
    TEST_CHECK(gameplay.ptr->enter_count == 1);

    // A finished run --transition--> Results (GameplayScreen's documented edge).
    gameplay.ptr->transition_on_update = true;
    gameplay.ptr->transition_target = ScreenId::Results;
    manager.update(kDt, {});
    TEST_CHECK(manager.active_id() == ScreenId::Results);
    TEST_CHECK(results.ptr->enter_count == 1);

    // Results --Back--> Select; Select --Back--> Title. No dead end.
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(manager.active_id() == ScreenId::Title);

    // Every non-Title canonical screen consumes Back (an exit edge); Title's
    // exit edges are Confirm -> Select (above) and the App-level Escape-quit.
    manager.start(ScreenId::Attract);
    TEST_CHECK(manager.back_navigates());
    manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(manager.active_id() != ScreenId::Attract);

    manager.start(ScreenId::Select);
    TEST_CHECK(manager.back_navigates());
    manager.start(ScreenId::Gameplay);
    TEST_CHECK(manager.back_navigates());
    manager.start(ScreenId::Results);
    TEST_CHECK(manager.back_navigates());

    std::cout << "  - arcade loop has no dead ends ok.\n";
}

} // namespace

int main() {
    std::cout << "[screen_manager_test] Running ScreenManager unit tests...\n";
    test_boot_lifecycle();
    test_transition_ordering();
    test_deferred_application();
    test_inactive_screens_ticked();
    test_unregistered_target();
    test_idle_attract_policy();
    test_idle_only_from_title_select();
    test_idle_disable();
    test_attract_return();
    test_back_navigation();
    test_calibration_back_navigation();
    test_input_remap_back_navigation();
    test_results_back_navigation();
    test_render_dispatch_headless();
    test_real_screens();
    test_font_sanity();
    test_app_shell_smoke();
    test_arcade_loop_no_dead_ends();
    std::cout << "[screen_manager_test] All tests passed!\n";
    return 0;
}