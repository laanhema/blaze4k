#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "chart/chart.hpp"
#include "chart/simfile_parser.hpp"
#include "gameplay/gameplay_view.hpp"
#include "gameplay/judgment.hpp"
#include "gameplay/judgment_engine.hpp"
#include "gameplay/life_keeper.hpp"
#include "timing/judgment_constants.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

bool approx(double a, double b, double epsilon = 1e-9) {
    return std::abs(a - b) < epsilon;
}

blaze4k::Note make_note(int column, double time_seconds, blaze4k::NoteType type,
                   double hold_end_time = 0.0) {
    blaze4k::Note note;
    note.column = column;
    note.beat = time_seconds; // row identity is the exact beat
    note.time_seconds = time_seconds;
    note.type = type;
    note.hold_end_time_seconds = hold_end_time;
    return note;
}

blaze4k::JudgmentEvent make_tap(int note_index, int column, blaze4k::TapJudgment window, double delta_ms) {
    blaze4k::JudgmentEvent event;
    event.kind = blaze4k::JudgmentKind::Tap;
    event.column = column;
    event.note_index = note_index;
    event.note_type = blaze4k::NoteType::Tap;
    event.window = window;
    event.delta_ms = delta_ms;
    return event;
}

blaze4k::JudgmentEvent make_miss(int note_index, int column) {
    blaze4k::JudgmentEvent event;
    event.kind = blaze4k::JudgmentKind::Miss;
    event.column = column;
    event.note_index = note_index;
    event.note_type = blaze4k::NoteType::Tap;
    event.window = blaze4k::TapJudgment::Miss;
    event.delta_ms = 180.0;
    return event;
}

blaze4k::JudgmentEvent make_hold_outcome(int note_index, int column, blaze4k::NoteType type,
                                    blaze4k::JudgmentKind kind, blaze4k::HoldJudgment hold) {
    blaze4k::JudgmentEvent event;
    event.kind = kind;
    event.column = column;
    event.note_index = note_index;
    event.note_type = type;
    event.hold = hold;
    return event;
}

blaze4k::JudgmentEvent make_mine(int note_index, int column) {
    blaze4k::JudgmentEvent event;
    event.kind = blaze4k::JudgmentKind::HitMine;
    event.column = column;
    event.note_index = note_index;
    event.note_type = blaze4k::NoteType::Mine;
    event.window = blaze4k::TapJudgment::HitMine;
    return event;
}

const blaze4k::JudgmentConstants& constants() {
    return blaze4k::JudgmentConstants::compiled_defaults();
}

std::array<bool, 4> held_none() {
    return std::array<bool, 4>{false, false, false, false};
}

std::array<bool, 4> held_col(int column) {
    std::array<bool, 4> held{false, false, false, false};
    held[static_cast<std::size_t>(column)] = true;
    return held;
}

// Builds `count` single-note tap rows at times 0,1,2,... cycling columns.
blaze4k::Chart tap_rows(int count) {
    blaze4k::Chart chart;
    for (int i = 0; i < count; ++i) {
        chart.notes.push_back(
            make_note(i % 4, static_cast<double>(i), blaze4k::NoteType::Tap));
    }
    chart.tap_count = count;
    return chart;
}

} // namespace

int main() {
    std::cout << "[life_keeper_test] Starting life/fail tests...\n";
    const blaze4k::JudgmentConstants& k = constants();

    // 1. Starts at the OpenITG InitialValue (0.5), not full.
    {
        blaze4k::Chart chart = tap_rows(1);
        blaze4k::LifeKeeper keeper;
        keeper.reset(&chart, &k);
        TEST_CHECK(approx(keeper.life(), 0.5));
        TEST_CHECK(!keeper.is_failing());
        TEST_CHECK(!keeper.has_failed());
        TEST_CHECK(keeper.fail_enabled());
        std::cout << "  - 1. life starts at 0.5 (OpenITG InitialValue).\n";
    }

    // 2. B2 life delta table at non-full life.
    {
        const auto single = [&k](blaze4k::TapJudgment window) {
            blaze4k::Chart chart = tap_rows(1);
            blaze4k::LifeKeeper keeper;
            keeper.reset(&chart, &k);
            keeper.consume(make_tap(0, 0, window, 0.0));
            return keeper.life();
        };
        TEST_CHECK(approx(single(blaze4k::TapJudgment::Fantastic), 0.508));
        TEST_CHECK(approx(single(blaze4k::TapJudgment::Excellent), 0.508));
        TEST_CHECK(approx(single(blaze4k::TapJudgment::Great), 0.504));
        TEST_CHECK(approx(single(blaze4k::TapJudgment::Decent), 0.5));
        TEST_CHECK(approx(single(blaze4k::TapJudgment::WayOff), 0.45));
        TEST_CHECK(approx(single(blaze4k::TapJudgment::Miss), 0.4));
        std::cout << "  - 2a. tap deltas match the B2 table (F/E +.008, G +.004, D 0, "
                     "W -.050, M -.100).\n";

        // Hit mine.
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 0.0, blaze4k::NoteType::Mine));
            chart.mine_count = 1;
            blaze4k::LifeKeeper keeper;
            keeper.reset(&chart, &k);
            keeper.consume(make_mine(0, 0));
            TEST_CHECK(approx(keeper.life(), 0.45));
        }
        // Hold/roll outcomes.
        const auto hold = [&k](blaze4k::JudgmentKind kind, blaze4k::HoldJudgment hj) {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 0.0, blaze4k::NoteType::HoldHead, 1.0));
            chart.hold_count = 1;
            blaze4k::LifeKeeper keeper;
            keeper.reset(&chart, &k);
            keeper.consume(make_hold_outcome(0, 0, blaze4k::NoteType::HoldHead, kind, hj));
            return keeper.life();
        };
        TEST_CHECK(approx(hold(blaze4k::JudgmentKind::HoldOk, blaze4k::HoldJudgment::Ok), 0.508));
        TEST_CHECK(approx(hold(blaze4k::JudgmentKind::HoldNg, blaze4k::HoldJudgment::Ng), 0.42));
        TEST_CHECK(approx(hold(blaze4k::JudgmentKind::RollOk, blaze4k::HoldJudgment::Ok), 0.508));
        TEST_CHECK(approx(hold(blaze4k::JudgmentKind::RollNg, blaze4k::HoldJudgment::Ng), 0.42));
        std::cout << "  - 2b. mine/OK/NG deltas match the B2 table.\n";
    }

    // 3. Row grouping: a 2-note jump changes life exactly once.
    {
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 1.0, blaze4k::NoteType::Tap));
            chart.notes.push_back(make_note(1, 1.0, blaze4k::NoteType::Tap));
            chart.tap_count = 2;
            blaze4k::LifeKeeper keeper;
            keeper.reset(&chart, &k);
            keeper.consume(make_tap(0, 0, blaze4k::TapJudgment::Great, -20.0));
            TEST_CHECK(approx(keeper.life(), 0.5)); // row not resolved yet
            keeper.consume(make_tap(1, 1, blaze4k::TapJudgment::Great, -10.0));
            TEST_CHECK(approx(keeper.life(), 0.504)); // one +0.004, not +0.008
        }
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 1.0, blaze4k::NoteType::Tap));
            chart.notes.push_back(make_note(1, 1.0, blaze4k::NoteType::Tap));
            chart.tap_count = 2;
            blaze4k::LifeKeeper keeper;
            keeper.reset(&chart, &k);
            keeper.consume(make_tap(0, 0, blaze4k::TapJudgment::Great, 0.0));
            keeper.consume(make_miss(1, 1));
            TEST_CHECK(approx(keeper.life(), 0.4)); // miss dominates: one -0.100
        }
        std::cout << "  - 3. jump rows apply exactly one life delta (miss dominates).\n";
    }

    // 4. Clamp.
    {
        blaze4k::Chart chart = tap_rows(100);
        blaze4k::LifeKeeper keeper;
        keeper.reset(&chart, &k);
        for (int i = 0; i < 100; ++i) {
            keeper.consume(make_tap(i, i % 4, blaze4k::TapJudgment::Fantastic, 0.0));
        }
        TEST_CHECK(approx(keeper.life(), 1.0));

        blaze4k::LifeKeeper dead;
        dead.reset(&chart, &k);
        for (int i = 0; i < 100; ++i) {
            dead.consume(make_miss(i, i % 4));
        }
        TEST_CHECK(approx(dead.life(), 0.0));
        std::cout << "  - 4. life clamps to [0,1].\n";
    }

    // 5. Fail enabled: empty bar fails, gameplay freezes.
    {
        blaze4k::Chart chart = tap_rows(8);
        blaze4k::LifeKeeper keeper;
        keeper.reset(&chart, &k);
        for (int i = 0; i < 6; ++i) {
            keeper.consume(make_miss(i, i % 4)); // 0.5 - 5*0.1 leaves ~2.8e-17; the
                                                 // 6th underflows past zero
        }
        TEST_CHECK(approx(keeper.life(), 0.0));
        TEST_CHECK(keeper.is_failing());
        TEST_CHECK(keeper.has_failed());
        keeper.consume(make_tap(6, 2, blaze4k::TapJudgment::Fantastic, 0.0));
        TEST_CHECK(approx(keeper.life(), 0.0)); // frozen after fail
        std::cout << "  - 5. fail-enabled freezes life at 0.\n";
    }

    // 6. Fail-Off: never fails, can recover (after the regen debt is repaid).
    {
        blaze4k::Chart chart = tap_rows(15);
        blaze4k::LifeKeeper keeper;
        keeper.set_fail_enabled(false);
        keeper.reset(&chart, &k);
        for (int i = 0; i < 5; ++i) {
            keeper.consume(make_miss(i, i % 4));
        }
        TEST_CHECK(approx(keeper.life(), 0.0));
        TEST_CHECK(!keeper.has_failed());
        // Five losses accumulate the debt to MaxRegenComboAfterMiss=10 (and the
        // crossing loss bumps it to 10): the first nine wins are suppressed, the
        // 10th pays.
        for (int i = 5; i < 15; ++i) {
            keeper.consume(make_tap(i, i % 4, blaze4k::TapJudgment::Fantastic, 0.0));
        }
        TEST_CHECK(approx(keeper.life(), 0.008));
        std::cout << "  - 6. fail-off recovers above zero without failing.\n";
    }

    // 7. Merciful drain scales negative deltas by SCALE(life,0,1,0.5,1).
    {
        blaze4k::JudgmentConstants merciful = blaze4k::JudgmentConstants::compiled_defaults();
        merciful.life.merciful_drain = true;
        blaze4k::Chart chart = tap_rows(1);

        blaze4k::LifeKeeper on;
        on.reset(&chart, &merciful);
        on.consume(make_miss(0, 0));
        TEST_CHECK(approx(on.life(), 0.425)); // 0.5 - 0.1*0.75

        blaze4k::LifeKeeper off;
        off.reset(&chart, &k);
        off.consume(make_miss(0, 0));
        TEST_CHECK(approx(off.life(), 0.4));
        std::cout << "  - 7. merciful drain honored from the constants table.\n";
    }

    // 8. Holds/mines apply individually; avoided mine / roll re-hit are neutral.
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(0, 1.0, blaze4k::NoteType::HoldHead, 2.0));
        chart.notes.push_back(make_note(1, 3.0, blaze4k::NoteType::Mine));
        chart.hold_count = 1;
        chart.mine_count = 1;
        blaze4k::LifeKeeper keeper;
        keeper.reset(&chart, &k);
        keeper.consume(make_tap(0, 0, blaze4k::TapJudgment::Great, 0.0)); // row head +0.004
        TEST_CHECK(approx(keeper.life(), 0.504));
        keeper.consume(make_hold_outcome(0, 0, blaze4k::NoteType::HoldHead, blaze4k::JudgmentKind::HoldOk,
                                         blaze4k::HoldJudgment::Ok)); // +0.008
        TEST_CHECK(approx(keeper.life(), 0.512));

        const double before = keeper.life();
        blaze4k::JudgmentEvent avoided;
        avoided.kind = blaze4k::JudgmentKind::AvoidedMine;
        avoided.column = 1;
        avoided.note_index = 1;
        avoided.note_type = blaze4k::NoteType::Mine;
        keeper.consume(avoided);
        blaze4k::JudgmentEvent roll_hit;
        roll_hit.kind = blaze4k::JudgmentKind::RollHit;
        roll_hit.column = 0;
        roll_hit.note_index = 0;
        roll_hit.note_type = blaze4k::NoteType::HoldHead;
        keeper.consume(roll_hit);
        TEST_CHECK(approx(keeper.life(), before));
        std::cout << "  - 8. hold head + outcome both apply; avoided mine/re-hit neutral.\n";
    }

    // 9. Idempotence: duplicate events never double-apply.
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(0, 0.0, blaze4k::NoteType::Tap));
        chart.notes.push_back(make_note(1, 1.0, blaze4k::NoteType::HoldHead, 2.0));
        chart.tap_count = 1;
        chart.hold_count = 1;
        blaze4k::LifeKeeper keeper;
        keeper.reset(&chart, &k);
        keeper.consume(make_tap(0, 0, blaze4k::TapJudgment::Fantastic, 0.0));
        keeper.consume(make_tap(0, 0, blaze4k::TapJudgment::Fantastic, 0.0));
        TEST_CHECK(approx(keeper.life(), 0.508));

        keeper.consume(make_hold_outcome(1, 1, blaze4k::NoteType::HoldHead, blaze4k::JudgmentKind::HoldOk,
                                         blaze4k::HoldJudgment::Ok));
        keeper.consume(make_hold_outcome(1, 1, blaze4k::NoteType::HoldHead, blaze4k::JudgmentKind::HoldOk,
                                         blaze4k::HoldJudgment::Ok));
        TEST_CHECK(approx(keeper.life(), 0.516));
        std::cout << "  - 9. duplicate events are ignored (per-note guard).\n";
    }

    // 10. Incremental == batch.
    {
        blaze4k::Chart chart = tap_rows(4);
        std::vector<blaze4k::JudgmentEvent> events = {
            make_tap(0, 0, blaze4k::TapJudgment::Fantastic, 0.0),
            make_tap(1, 1, blaze4k::TapJudgment::Great, -20.0),
            make_miss(2, 2),
            make_mine(3, 3),
        };
        blaze4k::LifeKeeper incremental;
        incremental.reset(&chart, &k);
        for (const blaze4k::JudgmentEvent& event : events) {
            incremental.consume(event);
        }
        blaze4k::LifeKeeper batch;
        batch.reset(&chart, &k);
        batch.consume(events);
        TEST_CHECK(approx(incremental.life(), batch.life()));
        TEST_CHECK(incremental.has_failed() == batch.has_failed());
        std::cout << "  - 10. incremental and batch consumption agree.\n";
    }

    // 11. Hot downgrade: while the bar is full, WayOff/Miss/mine/NG force -0.10.
    {
        const auto full_keeper = [&k](const blaze4k::Chart& chart, int* next_index) {
            blaze4k::LifeKeeper keeper;
            keeper.reset(&chart, &k);
            int i = 0;
            while (keeper.life() < 1.0 && i < 200) {
                keeper.consume(make_tap(i, i % 4, blaze4k::TapJudgment::Fantastic, 0.0));
                ++i;
            }
            *next_index = i;
            return std::pair<blaze4k::LifeKeeper, int>(std::move(keeper), i);
        };

        {
            blaze4k::Chart chart = tap_rows(70);
            int next = 0;
            auto [keeper, idx] = full_keeper(chart, &next);
            TEST_CHECK(approx(keeper.life(), 1.0));
            // WayOff at full: base -0.050, hot -> -0.10.
            blaze4k::JudgmentEvent way = make_tap(idx, idx % 4, blaze4k::TapJudgment::WayOff, 150.0);
            keeper.consume(way);
            TEST_CHECK(approx(keeper.life(), 0.9));
        }
        {
            blaze4k::Chart chart = tap_rows(70);
            int next = 0;
            auto [keeper, idx] = full_keeper(chart, &next);
            blaze4k::JudgmentEvent mine = make_mine(idx, idx % 4);
            keeper.consume(mine);
            TEST_CHECK(approx(keeper.life(), 0.9));
        }
        {
            blaze4k::Chart chart;
            for (int i = 0; i < 70; ++i) {
                chart.notes.push_back(make_note(i % 4, i, blaze4k::NoteType::Tap));
            }
            chart.notes.push_back(make_note(0, 100.0, blaze4k::NoteType::HoldHead, 101.0));
            chart.tap_count = 70;
            chart.hold_count = 1;
            int next = 0;
            auto [keeper, idx] = full_keeper(chart, &next);
            TEST_CHECK(approx(keeper.life(), 1.0));
            keeper.consume(make_hold_outcome(70, 0, blaze4k::NoteType::HoldHead,
                                             blaze4k::JudgmentKind::HoldNg, blaze4k::HoldJudgment::Ng));
            TEST_CHECK(approx(keeper.life(), 0.9));
        }
        std::cout << "  - 11. hot downgrade overrides table deltas at full life.\n";
    }

    // 12. Regen: after a loss, positive gains are suppressed for the debt period.
    {
        blaze4k::Chart chart = tap_rows(10);
        blaze4k::LifeKeeper keeper;
        keeper.reset(&chart, &k);
        keeper.consume(make_miss(0, 0)); // life 0.4, debt = 5
        for (int i = 1; i <= 4; ++i) {
            keeper.consume(make_tap(i, i % 4, blaze4k::TapJudgment::Fantastic, 0.0));
        }
        TEST_CHECK(approx(keeper.life(), 0.4)); // still in debt
        keeper.consume(make_tap(5, 1, blaze4k::TapJudgment::Fantastic, 0.0));
        TEST_CHECK(approx(keeper.life(), 0.408)); // debt paid on the 5th win
        std::cout << "  - 12. combo-to-regain suppresses post-loss gains.\n";
    }

    // 13. Engine integration on a reference chart (hand-computed OpenITG arithmetic):
    //     start 0.5
    //       tap col0 Fantastic  -> row +0.008                 = 0.508
    //       tap col1 Miss       -> -0.100 (row), debt = 5     = 0.408
    //       hold head col2 Fant -> +0.008 but debt 5->4, sup  = 0.408
    //       hold col2 Ok        -> +0.008 but debt 4->3, sup  = 0.408
    //     No fail; not complete is false (all rows/holds resolved).
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(0, 1.0, blaze4k::NoteType::Tap));
        chart.notes.push_back(make_note(1, 2.0, blaze4k::NoteType::Tap));
        chart.notes.push_back(make_note(2, 3.0, blaze4k::NoteType::HoldHead, 5.0));
        chart.tap_count = 2;
        chart.hold_count = 1;

        blaze4k::JudgmentEngine engine;
        engine.reset(&chart, &k);
        blaze4k::LifeKeeper keeper;
        keeper.reset(&chart, &k);

        engine.handle_step(0, 1.0);
        engine.update(2.5, held_none()); // col1 expires as Miss
        engine.handle_step(2, 3.0);      // hold head Fantastic
        engine.update(5.0, held_col(2)); // hold Ok

        std::vector<blaze4k::JudgmentEvent> drained;
        engine.drain_new_events(drained);
        keeper.consume(drained);

        TEST_CHECK(approx(keeper.life(), 0.408));
        TEST_CHECK(!keeper.has_failed());
        std::cout << "  - 13. life derives from an engine-produced B4 log.\n";
    }

    // 14. Fail-Off end-to-end through the engine, contrasted with fail-enabled.
    //     Notes 1..6 are missed; notes at 10..19 are then hit as Fantastics.
    {
        const auto build = [] {
            blaze4k::Chart chart;
            for (int i = 1; i <= 6; ++i) {
                chart.notes.push_back(make_note(i % 4, static_cast<double>(i), blaze4k::NoteType::Tap));
            }
            for (int i = 0; i < 10; ++i) {
                chart.notes.push_back(make_note(i % 4, 10.0 + static_cast<double>(i), blaze4k::NoteType::Tap));
            }
            chart.tap_count = 16;
            return chart;
        };

        const auto run = [&](bool fail_enabled) {
            blaze4k::Chart chart = build();
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            blaze4k::LifeKeeper keeper;
            keeper.set_fail_enabled(fail_enabled);
            keeper.reset(&chart, &k);

            engine.update(7.0, held_none()); // expire notes 1..6 as Miss
            std::vector<blaze4k::JudgmentEvent> drained;
            engine.drain_new_events(drained);
            keeper.consume(drained);

            for (int i = 0; i < 10; ++i) {
                engine.handle_step(i % 4, 10.0 + static_cast<double>(i));
            }
            drained.clear();
            engine.drain_new_events(drained);
            keeper.consume(drained);
            return std::make_pair(keeper.life(), keeper.has_failed());
        };

        const auto failed_run = run(true);
        TEST_CHECK(approx(failed_run.first, 0.0));
        TEST_CHECK(failed_run.second);

        const auto off_run = run(false);
        // Misses accumulate the debt to 10: nine wins suppressed, the 10th pays.
        TEST_CHECK(approx(off_run.first, 0.008));
        TEST_CHECK(!off_run.second);
        std::cout << "  - 14. fail-on fails at zero; fail-off recovers and survives.\n";
    }

    // 15. GameplayView boundary: life drains from the drained judgment log and the
    //     fail/outcome signal flips.
    {
        blaze4k::Chart chart;
        for (int i = 1; i <= 6; ++i) {
            chart.notes.push_back(make_note(i % 4, static_cast<double>(i), blaze4k::NoteType::Tap));
        }
        chart.tap_count = 6;

        blaze4k::GameplayView view;
        blaze4k::GameplayOptions options;
        TEST_CHECK(view.init(chart, k, "", options));
        TEST_CHECK(view.is_ready());
        TEST_CHECK(approx(view.life(), 0.5));
        for (int i = 0; i < 120; ++i) { // 2 s: notes 1..2 expire first
            view.update(1.0 / 60.0, held_none());
        }
        TEST_CHECK(view.life() < 0.5);
        TEST_CHECK(view.outcome() == blaze4k::GameplayOutcome::InProgress);
        view.shutdown();
        std::cout << "  - 15a. GameplayView drains life from the judgment log.\n";

        blaze4k::GameplayView failing;
        blaze4k::GameplayOptions fail_options;
        TEST_CHECK(failing.init(chart, k, "", fail_options));
        for (int i = 0; i < 600; ++i) { // 10 s: all 5 rows expire -> life 0
            failing.update(1.0 / 60.0, held_none());
        }
        TEST_CHECK(failing.has_failed());
        TEST_CHECK(failing.outcome() == blaze4k::GameplayOutcome::Failed);
        failing.shutdown();
        std::cout << "  - 15b. fail-enabled GameplayView reports Failed.\n";

        blaze4k::GameplayView off;
        blaze4k::GameplayOptions off_options;
        off_options.fail_enabled = false;
        TEST_CHECK(off.init(chart, k, "", off_options));
        for (int i = 0; i < 600; ++i) {
            off.update(1.0 / 60.0, held_none());
        }
        TEST_CHECK(!off.has_failed());
        TEST_CHECK(off.outcome() == blaze4k::GameplayOutcome::Cleared); // completed, no fail
        off.shutdown();
        std::cout << "  - 15c. fail-off GameplayView completes as Cleared.\n";
    }

    // 16. Null/empty chart is safe.
    {
        blaze4k::LifeKeeper keeper;
        keeper.reset(nullptr, &k);
        keeper.consume(make_tap(0, 0, blaze4k::TapJudgment::Fantastic, 0.0));
        TEST_CHECK(approx(keeper.life(), 0.5));
        TEST_CHECK(!keeper.has_failed());

        blaze4k::Chart empty;
        keeper.reset(&empty, &k);
        TEST_CHECK(approx(keeper.life(), 0.5));
        std::cout << "  - 16. null/empty chart is safe.\n";
    }

    // 17. Debt accumulates across successive losses up to MaxRegenComboAfterMiss.
    {
        blaze4k::Chart chart = tap_rows(14);
        blaze4k::LifeKeeper keeper;
        keeper.set_fail_enabled(false);
        keeper.reset(&chart, &k);
        keeper.consume(make_miss(0, 0)); // life 0.4, debt -> 5
        keeper.consume(make_miss(1, 1)); // life 0.3, debt -> 10
        for (int i = 2; i < 11; ++i) {   // nine wins suppressed
            keeper.consume(make_tap(i, i % 4, blaze4k::TapJudgment::Fantastic, 0.0));
        }
        TEST_CHECK(approx(keeper.life(), 0.3));
        keeper.consume(make_tap(11, 3, blaze4k::TapJudgment::Fantastic, 0.0)); // 10th pays
        TEST_CHECK(approx(keeper.life(), 0.308));
        std::cout << "  - 17. successive losses accumulate the regain debt to 10.\n";
    }

    // 18. Fail is evaluated once per consumed batch: a hit after a would-be-fatal
    //     miss in the same batch is not frozen out, matching OpenITG's once-per-frame
    //     bFailed check (ScreenGameplay.cpp:1471-1493). Regen is zeroed so the debt
    //     cannot suppress the rescuing hit.
    {
        blaze4k::JudgmentConstants custom = blaze4k::JudgmentConstants::compiled_defaults();
        custom.life.miss = -0.5;
        custom.life.regen_combo_after_miss = 0;
        custom.life.max_regen_combo_after_miss = 0;
        custom.life.regen_combo_after_fail = 0;
        custom.life.max_regen_combo_after_fail = 0;

        blaze4k::Chart chart = tap_rows(2);
        blaze4k::LifeKeeper keeper;
        keeper.reset(&chart, &custom);
        std::vector<blaze4k::JudgmentEvent> batch = {
            make_miss(0, 0),                                 // life 0.5 -> 0.0
            make_tap(1, 1, blaze4k::TapJudgment::Fantastic, 0.0), // rescued in the same batch
        };
        keeper.consume(batch);
        TEST_CHECK(approx(keeper.life(), 0.008));
        TEST_CHECK(!keeper.has_failed());
        std::cout << "  - 18. fail is decided after the whole batch (same-frame rescue).\n";
    }

    // 19. MercifulBeginner leaves life untouched (#67): OpenITG LifeMeterBar.cpp
    //     has no Beginner branch, so the same Miss / Way Off drains the same
    //     amount on a Beginner chart as on a Medium one.
    {
        blaze4k::Chart beginner_chart;
        beginner_chart.difficulty = "Beginner";
        beginner_chart.notes.push_back(make_note(0, 1.0, blaze4k::NoteType::Tap));
        beginner_chart.tap_count = 1;
        blaze4k::Chart medium_chart = beginner_chart;
        medium_chart.difficulty = "Medium";
        TEST_CHECK(beginner_chart.is_beginner());
        TEST_CHECK(!medium_chart.is_beginner());

        const blaze4k::JudgmentEvent cases[] = {
            make_miss(0, 0),
            make_tap(0, 0, blaze4k::TapJudgment::WayOff, 150.0),
        };
        for (const blaze4k::JudgmentEvent& event : cases) {
            blaze4k::LifeKeeper beginner;
            beginner.reset(&beginner_chart, &k);
            beginner.consume(event);
            blaze4k::LifeKeeper medium;
            medium.reset(&medium_chart, &k);
            medium.consume(event);
            TEST_CHECK(beginner.life() < 0.5);
            TEST_CHECK(approx(beginner.life(), medium.life()));
            TEST_CHECK(beginner.has_failed() == medium.has_failed());
        }
        std::cout << "  - 19. life is identical on Beginner (no MercifulBeginner branch).\n";
    }

    std::cout << "[life_keeper_test] All life/fail tests passed successfully!\n";
    return 0;
}
