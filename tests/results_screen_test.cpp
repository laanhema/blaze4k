#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "chart/chart.hpp"
#include "chart/note.hpp"
#include "chart/song.hpp"
#include "data/config.hpp"
#include "data/high_scores.hpp"
#include "gameplay/gameplay_options.hpp"
#include "render/gl_quad_renderer.hpp"
#include "screens/gameplay_screen.hpp"
#include "screens/play_request.hpp"
#include "screens/results.hpp"
#include "screens/results_screen.hpp"
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

using blaze4k::Chart;
using blaze4k::GameAction;
using blaze4k::HighScores;
using blaze4k::InputEvent;
using blaze4k::Note;
using blaze4k::NoteType;
using blaze4k::ResultsSummary;
using blaze4k::ScreenContext;
using blaze4k::ScreenId;
using blaze4k::Song;

constexpr double kDt = 0.1;

InputEvent press(GameAction action) {
    InputEvent event;
    event.action = action;
    event.pressed = true;
    return event;
}

Song make_song() {
    Song song;
    song.simfile_path = "Blaze Anthem.sm";
    song.metadata.title = "Blaze Anthem";
    song.metadata.artist = "Test Artist";
    return song;
}

Chart make_chart() {
    Chart chart;
    chart.steps_type = "dance-single";
    chart.difficulty = "Hard";
    chart.meter = 9;
    Note note;
    note.column = 0;
    note.beat = 1.0;
    note.type = NoteType::Tap;
    chart.notes.push_back(note);
    chart.tap_count = 1;
    return chart;
}

ResultsSummary make_summary(const Song& song, const Chart& chart, const std::string& grade,
                            double percent, bool failed = false) {
    ResultsSummary summary;
    summary.valid = true;
    summary.song = &song;
    summary.chart = &chart;
    summary.failed = failed;
    summary.grade_label = grade;
    summary.percent = percent;
    summary.actual_dp = 40;
    summary.possible_dp = 50;
    summary.max_combo = 7;
    return summary;
}

class SelectSpy : public blaze4k::Screen {
public:
    explicit SelectSpy(int* enters = nullptr) : enters_(enters) {}
    [[nodiscard]] ScreenId id() const override { return ScreenId::Select; }
    void enter(ScreenContext& /*ctx*/) override {
        if (enters_ != nullptr) {
            ++(*enters_);
        }
    }

private:
    int* enters_ = nullptr;
};

struct ResultsFixture {
    blaze4k::GameConfig config;
    HighScores scores;
    Song song = make_song();
    Chart chart = make_chart();
    ResultsSummary summary;
    blaze4k::ScreenManager manager{0.0};
    blaze4k::ResultsScreen* results = nullptr;
    int select_enters = 0;

    ResultsFixture() : summary(make_summary(song, chart, "S+", 0.95)) {}

    void start(bool with_results = true) {
        auto owner = std::make_unique<blaze4k::ResultsScreen>();
        results = owner.get();
        manager.add_screen(std::move(owner));
        manager.add_screen(std::make_unique<SelectSpy>(&select_enters));
        manager.context().config = &config;
        manager.context().scores = &scores;
        manager.context().results = with_results ? &summary : nullptr;
        manager.start(ScreenId::Results);
    }
};

// 1. Enter + submit + NEW RECORD on a first clear.
void test_enter_submit_and_flag() {
    ResultsFixture fx;
    fx.start();

    TEST_CHECK(fx.results->valid());
    TEST_CHECK(fx.results->submitted());
    TEST_CHECK(fx.results->new_record());
    TEST_CHECK(fx.results->summary().grade_label == "S+");

    const std::string key = blaze4k::make_chart_key(fx.song, fx.chart);
    const blaze4k::ScoreRecord* record = blaze4k::find_high_score(fx.scores, key);
    TEST_CHECK(record != nullptr);
    TEST_CHECK(record->grade == "S+");
    TEST_CHECK(record->percent == 0.95);
    std::cout << "  - enter submits + flags the first clear ok.\n";
}

// 2. A worse-than-best run submits but is not a new record; the table is unchanged.
void test_not_a_record() {
    ResultsFixture fx;
    TEST_CHECK(blaze4k::results_submit_score(fx.scores, make_summary(fx.song, fx.chart, "quad_star", 1.0),
                                        1));
    fx.start();

    TEST_CHECK(fx.results->submitted());
    TEST_CHECK(!fx.results->new_record());

    const std::string key = blaze4k::make_chart_key(fx.song, fx.chart);
    TEST_CHECK(blaze4k::find_high_score(fx.scores, key)->percent == 1.0);
    TEST_CHECK(blaze4k::find_high_score(fx.scores, key)->timestamp_unix == 1);
    std::cout << "  - a sub-best run does not flag a record ok.\n";
}

// 3. A failed run shows its stats, submits nothing, and never flags (OQ2/AC4).
void test_failed_run_no_submit() {
    ResultsFixture fx;
    fx.summary = make_summary(fx.song, fx.chart, "D", 0.30, true);
    fx.start();

    TEST_CHECK(fx.results->valid());
    TEST_CHECK(fx.results->summary().failed);
    TEST_CHECK(!fx.results->submitted());
    TEST_CHECK(!fx.results->new_record());
    TEST_CHECK(fx.scores.scores.empty());
    std::cout << "  - failed run submits nothing ok.\n";
}

// 3b. A valid, clearing run that cannot be stored (empty grade label) must not
//     report `submitted` and must not insert a record.
void test_unstorable_run_not_submitted() {
    ResultsFixture fx;
    fx.summary = make_summary(fx.song, fx.chart, "", 0.95);
    fx.start();

    TEST_CHECK(fx.results->valid());
    TEST_CHECK(!fx.results->summary().failed);
    TEST_CHECK(!fx.results->submitted());
    TEST_CHECK(!fx.results->new_record());
    TEST_CHECK(fx.scores.scores.empty());
    std::cout << "  - unstorable clear does not report submitted ok.\n";
}

// 4. Confirm skips the reveal, then returns to the song wheel (AC3).
void test_confirm_returns_to_wheel() {
    ResultsFixture fx;
    fx.start();
    TEST_CHECK(fx.manager.active_id() == ScreenId::Results);
    TEST_CHECK(!fx.results->reveal_finished());

    // First Confirm skips to the final frame without navigating (AC3).
    fx.manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(fx.manager.active_id() == ScreenId::Results);
    TEST_CHECK(fx.results->reveal_finished());

    // Second Confirm exits to the wheel (C7 behavior).
    fx.manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(fx.manager.active_id() == ScreenId::Select);
    TEST_CHECK(fx.select_enters == 1);
    std::cout << "  - Confirm skips then exits ok.\n";
}

// 4b. Options/Right also skip while the reveal runs; only a finished reveal navigates.
void test_reveal_gating_skip_presses() {
    ResultsFixture fx;
    fx.start();
    TEST_CHECK(!fx.results->reveal_finished());

    fx.manager.update(kDt, {press(GameAction::Options)});
    TEST_CHECK(fx.manager.active_id() == ScreenId::Results);
    TEST_CHECK(fx.results->reveal_finished());

    fx.manager.update(kDt, {press(GameAction::Right)});
    TEST_CHECK(fx.manager.active_id() == ScreenId::Select);
    std::cout << "  - Options skips, Right exits ok.\n";
}

// 4c. The NEW RECORD finale flag follows C7's best-score rule, and the screen's
//     single render gate (`shows_record_finale`) is false when it must not draw.
void test_new_record_finale_flag() {
    // A first clear plays the full finale.
    ResultsFixture first;
    first.start();
    TEST_CHECK(first.results->animator().new_record());
    TEST_CHECK(first.results->shows_record_finale());

    // A worse-than-best run does not, so the banner/flash is never drawn.
    ResultsFixture best;
    TEST_CHECK(blaze4k::results_submit_score(
        best.scores, make_summary(best.song, best.chart, "quad_star", 1.0), 1));
    best.start();
    TEST_CHECK(!best.results->animator().new_record());
    TEST_CHECK(!best.results->shows_record_finale());

    // A failed run never plays it (nor draws the finale).
    ResultsFixture failed;
    failed.summary = make_summary(failed.song, failed.chart, "D", 0.30, true);
    failed.start();
    TEST_CHECK(!failed.results->animator().new_record());
    TEST_CHECK(!failed.results->shows_record_finale());
    std::cout << "  - NEW RECORD finale flag gating ok.\n";
}

// 4d. An invalid (NO RESULT) summary has no reveal: a single Confirm/Options/Right
//     exits immediately (exactly C7) instead of being swallowed as a skip.
void test_invalid_summary_exits_immediately() {
    ResultsFixture fx;
    fx.start(false); // no published result -> invalid summary
    TEST_CHECK(!fx.results->valid());
    TEST_CHECK(!fx.results->shows_record_finale());
    TEST_CHECK(!fx.results->reveal_finished());

    fx.manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(fx.manager.active_id() == ScreenId::Select);
    TEST_CHECK(fx.select_enters == 1);

    // Options and Right behave the same on a fresh invalid screen.
    ResultsFixture options;
    options.start(false);
    options.manager.update(kDt, {press(GameAction::Options)});
    TEST_CHECK(options.manager.active_id() == ScreenId::Select);

    ResultsFixture right;
    right.start(false);
    right.manager.update(kDt, {press(GameAction::Right)});
    TEST_CHECK(right.manager.active_id() == ScreenId::Select);
    std::cout << "  - invalid summary exits on first press ok.\n";
}

// 5. Back goes through the manager default to the wheel (AC3, no dead end).
void test_back_returns_to_wheel() {
    ResultsFixture fx;
    fx.start();
    TEST_CHECK(fx.manager.back_navigates());

    fx.manager.update(kDt, {press(GameAction::Back)});
    TEST_CHECK(fx.manager.active_id() == ScreenId::Select);
    std::cout << "  - Back -> Select ok.\n";
}

// 6. Headless render is a no-op; re-entering with no handoff is invalid + safe.
void test_render_and_reenter() {
    ResultsFixture fx;
    fx.start();

    blaze4k::GlQuadRenderer renderer; // uninitialized: draws are no-ops
    fx.manager.render(renderer, 1280, 720);
    TEST_CHECK(fx.results->valid());

    fx.manager.context().results = nullptr;
    fx.manager.start(ScreenId::Results); // re-enter with no published result
    TEST_CHECK(!fx.results->valid());
    TEST_CHECK(!fx.results->submitted());
    fx.manager.render(renderer, 1280, 720);
    std::cout << "  - headless render + re-enter robustness ok.\n";
}

// 7. End-to-end: a real GameplayScreen run reports to Results, which submits and
//    returns to Select on Confirm (mirrors the headless completed-run pattern).
void test_gameplay_to_results_end_to_end() {
    blaze4k::GameConfig config;
    HighScores scores;
    Song song = make_song();
    Chart chart = make_chart();

    blaze4k::PlayRequest request;
    request.song = &song;
    request.chart = &chart;
    request.options = blaze4k::GameplayOptions{};

    ResultsSummary summary;
    blaze4k::ScreenManager manager(0.0);

    auto gameplay_owner = std::make_unique<blaze4k::GameplayScreen>();
    blaze4k::GameplayScreen* gameplay = gameplay_owner.get();
    auto results_owner = std::make_unique<blaze4k::ResultsScreen>();
    blaze4k::ResultsScreen* results = results_owner.get();

    manager.add_screen(std::move(gameplay_owner));
    manager.add_screen(std::move(results_owner));
    manager.add_screen(std::make_unique<SelectSpy>());
    manager.context().config = &config;
    manager.context().scores = &scores;
    manager.context().play_request = &request;
    manager.context().results = &summary;
    manager.start(ScreenId::Gameplay);
    TEST_CHECK(gameplay->is_ready());

    // Hit the single tap on the first frame (its time is 0.0) so the run clears
    // with a non-negative percent; a full miss would be negative and is rejected
    // by the percent<0 submit guard.
    manager.update(1.0 / 60.0, {press(GameAction::Left)});
    for (int i = 0; i < 180 && manager.active_id() != ScreenId::Results; ++i) {
        manager.update(1.0 / 60.0, {});
    }

    TEST_CHECK(manager.active_id() == ScreenId::Results);
    TEST_CHECK(gameplay->end_reported());
    TEST_CHECK(summary.valid);
    TEST_CHECK(summary.song == &song);
    TEST_CHECK(summary.chart == &chart);
    TEST_CHECK(results->valid());
    TEST_CHECK(results->submitted());

    const std::string key = blaze4k::make_chart_key(song, chart);
    TEST_CHECK(blaze4k::find_high_score(scores, key) != nullptr);
    TEST_CHECK(scores.scores.size() == 1);

    // Extra frames on Results must not re-enter or resubmit (exactly-once handoff).
    for (int i = 0; i < 30; ++i) {
        manager.update(1.0 / 60.0, {});
    }
    TEST_CHECK(manager.active_id() == ScreenId::Results);
    TEST_CHECK(scores.scores.size() == 1);
    TEST_CHECK(gameplay->end_reported());

    // The reveal is running on entry: the first Confirm skips it in place...
    manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(manager.active_id() == ScreenId::Results);
    TEST_CHECK(results->reveal_finished());

    // ...and the second Confirm returns to the wheel (C7 behavior).
    manager.update(kDt, {press(GameAction::Confirm)});
    TEST_CHECK(manager.active_id() == ScreenId::Select);

    // Frames after returning to the wheel must not resurrect the run or resubmit.
    for (int i = 0; i < 30; ++i) {
        manager.update(1.0 / 60.0, {});
    }
    TEST_CHECK(scores.scores.size() == 1);
    TEST_CHECK(gameplay->end_reported());
    std::cout << "  - Gameplay -> Results -> Select end-to-end ok.\n";
}

} // namespace

int main() {
    std::cout << "[results_screen_test] Running ResultsScreen tests...\n";
    test_enter_submit_and_flag();
    test_not_a_record();
    test_failed_run_no_submit();
    test_unstorable_run_not_submitted();
    test_confirm_returns_to_wheel();
    test_reveal_gating_skip_presses();
    test_new_record_finale_flag();
    test_invalid_summary_exits_immediately();
    test_back_returns_to_wheel();
    test_render_and_reenter();
    test_gameplay_to_results_end_to_end();
    std::cout << "[results_screen_test] All tests passed!\n";
    return 0;
}
