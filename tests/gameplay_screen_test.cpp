#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "chart/chart.hpp"
#include "chart/song.hpp"
#include "chart/timing_data.hpp"
#include "gameplay/hud_renderer.hpp"
#include "screens/gameplay_screen.hpp"
#include "screens/play_request.hpp"
#include "screens/screen.hpp"
#include "screens/screen_manager.hpp"
#include "render/gl_quad_renderer.hpp"
#include "render/theme.hpp"
#include "render/theme_textures.hpp"
#include "render/ttf_font.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

using blaze4k::ScreenContext;
using blaze4k::ScreenId;

// Minimal destination screen so the manager can apply Gameplay's transition.
class StubScreen : public blaze4k::Screen {
public:
    explicit StubScreen(ScreenId id) : id_(id) {}
    [[nodiscard]] ScreenId id() const override { return id_; }

private:
    ScreenId id_;
};

blaze4k::InputEvent press(blaze4k::GameAction action) {
    blaze4k::InputEvent event;
    event.action = action;
    event.pressed = true;
    return event;
}

blaze4k::Note make_tap(int column, double beat, double time_seconds) {
    blaze4k::Note note;
    note.column = column;
    note.beat = beat;
    note.time_seconds = time_seconds;
    note.type = blaze4k::NoteType::Tap;
    return note;
}

// A finished run must linger for the full end delay before the score screen,
// instead of snapping over the instant the last note resolves. A single unhit
// tap expires at music time > effective way_off (0.1815 s); with an exact 0.25 s timestep it
// misses on the first update, so the 2 s delay lands exactly on the 8th update.
void test_end_delay_before_results() {
    blaze4k::Song song;
    blaze4k::Chart chart;
    chart.timing.parse_bpms_string("0=120");
    chart.notes.push_back(make_tap(0, 0.0, 0.0));

    blaze4k::PlayRequest request;
    request.song = &song;
    request.chart = &chart;
    request.options.fail_enabled = false; // isolate the cleared path

    blaze4k::ScreenManager manager(0.0); // disable idle -> Attract
    auto gameplay_owner = std::make_unique<blaze4k::GameplayScreen>();
    blaze4k::GameplayScreen* gameplay = gameplay_owner.get();
    manager.add_screen(std::move(gameplay_owner));
    manager.add_screen(std::make_unique<StubScreen>(ScreenId::Select));
    manager.context().play_request = &request;

    manager.start(ScreenId::Gameplay);
    TEST_CHECK(manager.active_id() == ScreenId::Gameplay);

    constexpr double dt = 0.25;
    const int delay_updates =
        static_cast<int>(blaze4k::GameplayScreen::kEndDelaySeconds / dt);
    TEST_CHECK(std::abs(blaze4k::GameplayScreen::kEndDelaySeconds - delay_updates * dt) < 1e-9);

    // Updates 1..(delay_updates - 1): the run has ended but the field is held.
    for (int i = 0; i < delay_updates - 1; ++i) {
        manager.update(dt, {});
    }
    TEST_CHECK(manager.active_id() == ScreenId::Gameplay);
    TEST_CHECK(!gameplay->end_reported());

    // The update that completes the delay reports the end and transitions.
    manager.update(dt, {});
    TEST_CHECK(gameplay->end_reported());
    TEST_CHECK(manager.active_id() == ScreenId::Select);
    std::cout << "  - run end lingers 2 s before Results ok.\n";
}

// #112: Gameplay hides the OS cursor on enter and shows it on exit, through the
// null-guarded ScreenContext service, so every way out of a run restores it.
void test_cursor_hidden_during_gameplay() {
    using blaze4k::GameAction;
    using Calls = std::vector<bool>;

    blaze4k::Song song;
    blaze4k::Chart chart;
    chart.timing.parse_bpms_string("0=120");
    chart.notes.push_back(make_tap(0, 0.0, 0.0));

    blaze4k::PlayRequest request;
    request.song = &song;
    request.chart = &chart;
    request.options.fail_enabled = false;

    constexpr double dt = 0.25;
    const int end_updates = static_cast<int>(blaze4k::GameplayScreen::kEndDelaySeconds / dt);

    // 1. Run end -> Results. A failed run leaves through the exact same
    // transition_to line in GameplayScreen::update (gated only on
    // outcome() != InProgress), so this case covers the fail path's cursor
    // lifecycle too; the owner's windowed check covers a real fail.
    {
        Calls calls;
        blaze4k::ScreenManager manager(0.0);
        manager.add_screen(std::make_unique<blaze4k::GameplayScreen>());
        manager.add_screen(std::make_unique<StubScreen>(ScreenId::Results));
        manager.add_screen(std::make_unique<StubScreen>(ScreenId::Select));
        manager.context().play_request = &request;
        manager.context().set_cursor_visible = [&calls](bool v) { calls.push_back(v); };

        manager.start(ScreenId::Gameplay);
        TEST_CHECK((calls == Calls{false}));
        for (int i = 0; i < end_updates; ++i) {
            manager.update(dt, {});
        }
        manager.update(0.0, {}); // apply any deferred transition
        TEST_CHECK(manager.active_id() == ScreenId::Results);
        TEST_CHECK((calls == Calls{false, true}));
    }

    // 2. Back-abort -> Select, then 3. re-entry and leaving again: the calls
    // strictly alternate, so the hidden state never sticks.
    {
        Calls calls;
        blaze4k::ScreenManager manager(0.0);
        manager.add_screen(std::make_unique<blaze4k::GameplayScreen>());
        manager.add_screen(std::make_unique<StubScreen>(ScreenId::Select));
        manager.context().play_request = &request;
        manager.context().set_cursor_visible = [&calls](bool v) { calls.push_back(v); };

        manager.start(ScreenId::Gameplay);
        TEST_CHECK((calls == Calls{false}));
        manager.update(dt, {press(GameAction::Back)});
        manager.update(0.0, {});
        TEST_CHECK(manager.active_id() == ScreenId::Select);
        TEST_CHECK((calls == Calls{false, true}));

        manager.transition_to(ScreenId::Gameplay);
        manager.update(0.0, {});
        TEST_CHECK(manager.active_id() == ScreenId::Gameplay);
        TEST_CHECK((calls == Calls{false, true, false}));
        manager.update(dt, {press(GameAction::Back)});
        manager.update(0.0, {});
        TEST_CHECK(manager.active_id() == ScreenId::Select);
        TEST_CHECK((calls == Calls{false, true, false, true}));
    }

    // 4. No play request: the empty screen still hides and Back restores it.
    {
        Calls calls;
        blaze4k::ScreenManager manager(0.0);
        manager.add_screen(std::make_unique<blaze4k::GameplayScreen>());
        manager.add_screen(std::make_unique<StubScreen>(ScreenId::Select));
        manager.context().play_request = nullptr;
        manager.context().set_cursor_visible = [&calls](bool v) { calls.push_back(v); };

        manager.start(ScreenId::Gameplay);
        TEST_CHECK((calls == Calls{false}));
        manager.update(dt, {press(GameAction::Back)});
        manager.update(0.0, {});
        TEST_CHECK(manager.active_id() == ScreenId::Select);
        TEST_CHECK((calls == Calls{false, true}));
    }

    // 5. Null callback (headless / --gameplay-demo wiring): enter and exit do not crash.
    {
        blaze4k::ScreenManager manager(0.0);
        manager.add_screen(std::make_unique<blaze4k::GameplayScreen>());
        manager.add_screen(std::make_unique<StubScreen>(ScreenId::Select));
        manager.context().play_request = &request;
        TEST_CHECK(!manager.context().set_cursor_visible);

        manager.start(ScreenId::Gameplay);
        manager.update(dt, {press(GameAction::Back)});
        manager.update(0.0, {});
        TEST_CHECK(manager.active_id() == ScreenId::Select);
    }
    std::cout << "  - cursor hidden during gameplay, restored on every exit ok.\n";
}

bool same_colors(const blaze4k::theme::DifficultyColors& a,
                 const blaze4k::theme::DifficultyColors& b) {
    const auto eq = [](blaze4k::Color x, blaze4k::Color y) {
        return x.r == y.r && x.g == y.g && x.b == y.b && x.a == y.a;
    };
    return eq(a.fill, b.fill) && eq(a.ink, b.ink);
}

blaze4k::Chart make_chart(const std::string& difficulty, const std::string& description,
                          int meter) {
    blaze4k::Chart chart;
    chart.difficulty = difficulty;
    chart.description = description;
    chart.meter = meter;
    return chart;
}

// #93: label + meter with the select screen's label rule and colours.
void test_difficulty_badge_for() {
    namespace difficulty = blaze4k::theme::difficulty;

    const blaze4k::DifficultyBadge hard = blaze4k::difficulty_badge_for(make_chart("Hard", "", 8));
    TEST_CHECK(hard.label == "HARD" && hard.meter == "8");
    TEST_CHECK(same_colors(hard.colors, difficulty::kHard));

    const blaze4k::DifficultyBadge challenge =
        blaze4k::difficulty_badge_for(make_chart("Challenge", "", 12));
    TEST_CHECK(challenge.label == "CHALLENGE" && challenge.meter == "12");
    TEST_CHECK(same_colors(challenge.colors, difficulty::kChallenge));

    const blaze4k::DifficultyBadge named =
        blaze4k::difficulty_badge_for(make_chart("Edit", "Crazy Edit", 11));
    TEST_CHECK(named.label == "Crazy Edit" && named.meter == "11");
    TEST_CHECK(same_colors(named.colors, difficulty::kEdit));

    const blaze4k::DifficultyBadge edit = blaze4k::difficulty_badge_for(make_chart("Edit", "", 5));
    TEST_CHECK(edit.label == "EDIT" && edit.meter == "5");
    TEST_CHECK(same_colors(edit.colors, difficulty::kEdit));

    const blaze4k::DifficultyBadge unnamed = blaze4k::difficulty_badge_for(make_chart("", "", 1));
    TEST_CHECK(unnamed.label == "BEGINNER" && unnamed.meter == "1");
    TEST_CHECK(same_colors(unnamed.colors, difficulty::kBeginner));
    std::cout << "  - difficulty badge text and colours ok.\n";
}

// Review fix (#93): a 40-character Edit name, fitted with the real badge font at the
// 300 ref px plate cap (720p), is "..."-truncated but still ends with its meter.
void test_long_edit_badge_keeps_meter() {
    blaze4k::TextRenderer text;
    TEST_CHECK(text.load(std::filesystem::path{BLAZE4K_SOURCE_DIR}));
    text.set_window_size(1280, 720);
    const auto measure = [&text](std::string_view s) {
        return text.measure(s, blaze4k::theme::text::kBadge);
    };

    const std::string name = "An Extremely Long Edit Chart Name Here!!";
    TEST_CHECK(name.size() == 40);
    const blaze4k::DifficultyBadge badge =
        blaze4k::difficulty_badge_for(make_chart("Edit", name, 13));
    TEST_CHECK(badge.label == name && badge.meter == "13");

    const blaze4k::DiffBadgeLayout layout = blaze4k::layout_diff_badge(
        blaze4k::badge_text_width(badge, measure), 1280, 720, 424.0);
    TEST_CHECK(layout.visible);
    TEST_CHECK(std::abs(layout.plate.w - 300.0f) < 1e-3f); // capped
    const std::string drawn = blaze4k::fit_badge_text(badge, layout.text_max_w, measure);
    const std::string tail = "... 13";
    TEST_CHECK(drawn.size() > tail.size());
    TEST_CHECK(drawn.compare(drawn.size() - tail.size(), tail.size(), tail) == 0);
    TEST_CHECK(measure(drawn) <= layout.text_max_w + 1e-3f);
    text.shutdown();
    std::cout << "  - long Edit badge keeps its meter ok.\n";
}

// #93: entering Gameplay hands the view its badge, and a render with the real
// (headless) theme and fonts on an uninitialised renderer does not crash.
void test_enter_sets_badge_and_renders_headless() {
    const std::filesystem::path source_dir{BLAZE4K_SOURCE_DIR};
    const std::filesystem::path cabinet =
        std::filesystem::path{BLAZE4K_ASSETS_DIR} / "theme" / "cabinet";
    blaze4k::ThemeTextures theme;
    TEST_CHECK(theme.load(cabinet));
    blaze4k::TextRenderer text;
    TEST_CHECK(text.load(source_dir));
    text.set_window_size(1280, 720);

    blaze4k::Song song;
    blaze4k::Chart chart = make_chart("Hard", "", 8);
    chart.timing.parse_bpms_string("0=120");
    chart.notes.push_back(make_tap(0, 0.0, 0.0));

    blaze4k::PlayRequest request;
    request.song = &song;
    request.chart = &chart;
    request.options.fail_enabled = false;

    blaze4k::ScreenManager manager(0.0);
    auto gameplay_owner = std::make_unique<blaze4k::GameplayScreen>();
    blaze4k::GameplayScreen* gameplay = gameplay_owner.get();
    manager.add_screen(std::move(gameplay_owner));
    manager.add_screen(std::make_unique<StubScreen>(ScreenId::Select));
    manager.context().play_request = &request;
    manager.context().theme = &theme;
    manager.context().text = &text;

    manager.start(ScreenId::Gameplay);
    TEST_CHECK(manager.active_id() == ScreenId::Gameplay);
    TEST_CHECK(gameplay->difficulty_badge().label == "HARD");
    TEST_CHECK(gameplay->difficulty_badge().meter == "8");
    TEST_CHECK(same_colors(gameplay->difficulty_badge().colors, blaze4k::theme::difficulty::kHard));

    blaze4k::GlQuadRenderer renderer; // uninitialised: draws are no-ops
    manager.update(0.25, {});
    manager.render(renderer, 1280, 720);
    manager.render(renderer, 2560, 1440);
    manager.render(renderer, 0, 0);

    manager.transition_to(ScreenId::Select);
    manager.update(0.0, {});
    text.shutdown();
    theme.shutdown();
    std::cout << "  - enter sets the badge and renders headless ok.\n";
}

} // namespace

int main() {
    std::cout << "[gameplay_screen_test] Running gameplay screen tests...\n";
    test_end_delay_before_results();
    test_cursor_hidden_during_gameplay();
    test_difficulty_badge_for();
    test_long_edit_badge_keeps_meter();
    test_enter_sets_badge_and_renders_headless();
    std::cout << "[gameplay_screen_test] All tests passed!\n";
    return 0;
}
