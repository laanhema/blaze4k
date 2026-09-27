#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

#include "chart/chart.hpp"
#include "chart/song.hpp"
#include "chart/timing_data.hpp"
#include "screens/gameplay_screen.hpp"
#include "screens/play_request.hpp"
#include "screens/screen.hpp"
#include "screens/screen_manager.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

using td::ScreenContext;
using td::ScreenId;

// Minimal destination screen so the manager can apply Gameplay's transition.
class StubScreen : public td::Screen {
public:
    explicit StubScreen(ScreenId id) : id_(id) {}
    [[nodiscard]] ScreenId id() const override { return id_; }

private:
    ScreenId id_;
};

td::Note make_tap(int column, double beat, double time_seconds) {
    td::Note note;
    note.column = column;
    note.beat = beat;
    note.time_seconds = time_seconds;
    note.type = td::NoteType::Tap;
    return note;
}

// A finished run must linger for the full end delay before the score screen,
// instead of snapping over the instant the last note resolves. A single unhit
// tap expires at music time > way_off (0.18 s); with an exact 0.25 s timestep it
// misses on the first update, so the 2 s delay lands exactly on the 8th update.
void test_end_delay_before_results() {
    td::Song song;
    td::Chart chart;
    chart.timing.parse_bpms_string("0=120");
    chart.notes.push_back(make_tap(0, 0.0, 0.0));

    td::PlayRequest request;
    request.song = &song;
    request.chart = &chart;
    request.options.fail_enabled = false; // isolate the cleared path

    td::ScreenManager manager(0.0); // disable idle -> Attract
    auto gameplay_owner = std::make_unique<td::GameplayScreen>();
    td::GameplayScreen* gameplay = gameplay_owner.get();
    manager.add_screen(std::move(gameplay_owner));
    manager.add_screen(std::make_unique<StubScreen>(ScreenId::Select));
    manager.context().play_request = &request;

    manager.start(ScreenId::Gameplay);
    TEST_CHECK(manager.active_id() == ScreenId::Gameplay);

    constexpr double dt = 0.25;
    const int delay_updates =
        static_cast<int>(td::GameplayScreen::kEndDelaySeconds / dt);
    TEST_CHECK(std::abs(td::GameplayScreen::kEndDelaySeconds - delay_updates * dt) < 1e-9);

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

} // namespace

int main() {
    std::cout << "[gameplay_screen_test] Running gameplay screen tests...\n";
    test_end_delay_before_results();
    std::cout << "[gameplay_screen_test] All tests passed!\n";
    return 0;
}
