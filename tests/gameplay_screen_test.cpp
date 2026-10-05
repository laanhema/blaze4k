#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "chart/chart.hpp"
#include "chart/song.hpp"
#include "chart/timing_data.hpp"
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

// #93: "<LABEL> <meter>" with the select screen's label rule and colours.
void test_difficulty_badge_for() {
    namespace difficulty = blaze4k::theme::difficulty;

    const blaze4k::DifficultyBadge hard = blaze4k::difficulty_badge_for(make_chart("Hard", "", 8));
    TEST_CHECK(hard.text == "HARD 8");
    TEST_CHECK(same_colors(hard.colors, difficulty::kHard));

    const blaze4k::DifficultyBadge challenge =
        blaze4k::difficulty_badge_for(make_chart("Challenge", "", 12));
    TEST_CHECK(challenge.text == "CHALLENGE 12");
    TEST_CHECK(same_colors(challenge.colors, difficulty::kChallenge));

    const blaze4k::DifficultyBadge named =
        blaze4k::difficulty_badge_for(make_chart("Edit", "Crazy Edit", 11));
    TEST_CHECK(named.text == "Crazy Edit 11");
    TEST_CHECK(same_colors(named.colors, difficulty::kEdit));

    const blaze4k::DifficultyBadge edit = blaze4k::difficulty_badge_for(make_chart("Edit", "", 5));
    TEST_CHECK(edit.text == "EDIT 5");
    TEST_CHECK(same_colors(edit.colors, difficulty::kEdit));

    const blaze4k::DifficultyBadge unnamed = blaze4k::difficulty_badge_for(make_chart("", "", 1));
    TEST_CHECK(unnamed.text == "BEGINNER 1");
    TEST_CHECK(same_colors(unnamed.colors, difficulty::kBeginner));
    std::cout << "  - difficulty badge text and colours ok.\n";
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
    TEST_CHECK(gameplay->difficulty_badge().text == "HARD 8");
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
    test_difficulty_badge_for();
    test_enter_sets_badge_and_renders_headless();
    std::cout << "[gameplay_screen_test] All tests passed!\n";
    return 0;
}
