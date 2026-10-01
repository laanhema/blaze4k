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
#include "gameplay/hud_renderer.hpp"
#include "gameplay/judgment.hpp"
#include "gameplay/judgment_engine.hpp"
#include "gameplay/score_keeper.hpp"
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
    note.beat = time_seconds;
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

} // namespace

int main() {
    std::cout << "[score_keeper_test] Starting scoring tests...\n";
    const blaze4k::JudgmentConstants& k = constants();

    // 1. Possible DP denominator: jump (2 taps, 1 row) + hold + roll + mine.
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(0, 1.0, blaze4k::NoteType::Tap));
        chart.notes.push_back(make_note(1, 1.0, blaze4k::NoteType::Tap)); // jump with the above
        chart.notes.push_back(make_note(2, 2.0, blaze4k::NoteType::HoldHead, 3.0));
        chart.notes.push_back(make_note(3, 4.0, blaze4k::NoteType::RollHead, 5.0));
        chart.notes.push_back(make_note(0, 6.0, blaze4k::NoteType::Mine));
        chart.tap_count = 2;
        chart.hold_count = 1;
        chart.roll_count = 1;
        chart.mine_count = 1;

        blaze4k::ScoreKeeper keeper;
        keeper.reset(&chart, &k);
        // 3 rows (jump, hold, roll) * 5 + hold 5 + roll 5 == 25.
        TEST_CHECK(keeper.possible_dance_points() == 25);
        TEST_CHECK(keeper.actual_dance_points() == 0);
        TEST_CHECK(!keeper.is_complete());
        std::cout << "  - Denominator is chart-derived (rows*5 + holds*5 + rolls*5).\n";
    }

    // 2. Single-tap DP/percent for every window.
    {
        struct Case {
            blaze4k::TapJudgment window;
            int expected_dp;
            double expected_percent;
        };
        const Case cases[] = {
            {blaze4k::TapJudgment::Fantastic, 5, 1.0},
            {blaze4k::TapJudgment::Excellent, 4, 0.8},
            {blaze4k::TapJudgment::Great, 2, 0.4},
            {blaze4k::TapJudgment::Decent, 0, 0.0},
            {blaze4k::TapJudgment::WayOff, -6, -1.2},
            {blaze4k::TapJudgment::Miss, -12, -2.4},
        };
        for (const Case& c : cases) {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 1.0, blaze4k::NoteType::Tap));
            chart.tap_count = 1;
            blaze4k::ScoreKeeper keeper;
            keeper.reset(&chart, &k);
            if (c.window == blaze4k::TapJudgment::Miss) {
                keeper.consume(make_miss(0, 0));
            } else {
                keeper.consume(make_tap(0, 0, c.window, 0.0));
            }
            TEST_CHECK(keeper.actual_dance_points() == c.expected_dp);
            TEST_CHECK(approx(keeper.percent(), c.expected_percent));
        }
        std::cout << "  - DP weights and percent match OpenITG (Fantastic 5, Great 2).\n";
    }

    // 3. Grade mapping follows the B2 tiers.
    {
        const auto run_rows = [&k](const std::vector<blaze4k::TapJudgment>& windows) {
            blaze4k::Chart chart;
            for (std::size_t i = 0; i < windows.size(); ++i) {
                chart.notes.push_back(
                    make_note(static_cast<int>(i % 4), static_cast<double>(i), blaze4k::NoteType::Tap));
            }
            chart.tap_count = static_cast<int>(windows.size());
            blaze4k::ScoreKeeper keeper;
            keeper.reset(&chart, &k);
            for (std::size_t i = 0; i < windows.size(); ++i) {
                keeper.consume(make_tap(static_cast<int>(i), static_cast<int>(i % 4),
                                        windows[i], 0.0));
            }
            return std::make_pair(keeper.percent(), std::string(keeper.grade().label));
        };

        const auto expect_grade = [&](const std::vector<blaze4k::TapJudgment>& windows,
                                      double percent, const std::string& label) {
            const auto result = run_rows(windows);
            TEST_CHECK(approx(result.first, percent));
            TEST_CHECK(result.second == label);
        };

        expect_grade(std::vector<blaze4k::TapJudgment>(20, blaze4k::TapJudgment::Fantastic), 1.0, "quad_star");
        {
            std::vector<blaze4k::TapJudgment> w(20, blaze4k::TapJudgment::Fantastic);
            w[19] = blaze4k::TapJudgment::Excellent; // 95 + 4 = 99
            expect_grade(w, 0.99, "triple_star");
        }
        {
            std::vector<blaze4k::TapJudgment> w(20, blaze4k::TapJudgment::Fantastic);
            w[18] = blaze4k::TapJudgment::Excellent; // 90 + 8 = 98
            w[19] = blaze4k::TapJudgment::Excellent;
            expect_grade(w, 0.98, "double_star");
        }
        {
            std::vector<blaze4k::TapJudgment> w(20, blaze4k::TapJudgment::Fantastic);
            w[18] = blaze4k::TapJudgment::Excellent; // 90 + 4 + 2 = 96
            w[19] = blaze4k::TapJudgment::Great;
            expect_grade(w, 0.96, "single_star");
        }
        {
            std::vector<blaze4k::TapJudgment> w(20, blaze4k::TapJudgment::Fantastic);
            w[16] = blaze4k::TapJudgment::Excellent; // 80 + 12 + 2 = 94
            w[17] = blaze4k::TapJudgment::Excellent;
            w[18] = blaze4k::TapJudgment::Excellent;
            w[19] = blaze4k::TapJudgment::Great;
            expect_grade(w, 0.94, "S+");
        }
        {
            std::vector<blaze4k::TapJudgment> w(20, blaze4k::TapJudgment::Decent);
            for (int i = 0; i < 11; ++i) {
                w[static_cast<std::size_t>(i)] = blaze4k::TapJudgment::Fantastic; // 55
            }
            expect_grade(w, 0.55, "C-");
        }
        {
            std::vector<blaze4k::TapJudgment> w(20, blaze4k::TapJudgment::Decent); // 0
            expect_grade(w, 0.0, "D");
        }

        // grade() is exactly the B2 lookup on the (unclamped) percent.
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(0, 1.0, blaze4k::NoteType::Tap));
        chart.tap_count = 1;
        blaze4k::ScoreKeeper keeper;
        keeper.reset(&chart, &k);
        keeper.consume(make_tap(0, 0, blaze4k::TapJudgment::Great, 0.0));
        TEST_CHECK(&keeper.grade() == &k.grade_for_percent(keeper.percent()));
        std::cout << "  - Grade thresholds match the OpenITG arcade tiers.\n";
    }

    // 4. Row aggregation: a 2-note jump scores/counts once.
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(0, 1.0, blaze4k::NoteType::Tap));
        chart.notes.push_back(make_note(1, 1.0, blaze4k::NoteType::Tap));
        chart.tap_count = 2;
        blaze4k::ScoreKeeper keeper;
        keeper.reset(&chart, &k);
        TEST_CHECK(keeper.possible_dance_points() == 5);

        keeper.consume(make_tap(0, 0, blaze4k::TapJudgment::Great, -20.0));
        TEST_CHECK(!keeper.is_complete()); // row not fully judged yet
        TEST_CHECK(keeper.actual_dance_points() == 0);
        keeper.consume(make_tap(1, 1, blaze4k::TapJudgment::Great, -10.0));

        TEST_CHECK(keeper.actual_dance_points() == 2); // exactly one Great weight
        TEST_CHECK(keeper.state().tap_counts[static_cast<std::size_t>(blaze4k::TapJudgment::Great)] == 1);
        TEST_CHECK(keeper.state().combo == 2);
        TEST_CHECK(keeper.is_complete());
        std::cout << "  - Jump rows score once and add the row's note count to combo.\n";
    }

    // 5. Last-tap semantics: latest offset wins; a miss dominates the row.
    {
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 1.0, blaze4k::NoteType::Tap));
            chart.notes.push_back(make_note(1, 1.0, blaze4k::NoteType::Tap));
            chart.tap_count = 2;
            blaze4k::ScoreKeeper keeper;
            keeper.reset(&chart, &k);
            keeper.consume(make_tap(0, 0, blaze4k::TapJudgment::Great, -50.0)); // early
            keeper.consume(make_tap(1, 1, blaze4k::TapJudgment::Excellent, 10.0)); // later
            TEST_CHECK(keeper.actual_dance_points() == 4); // Excellent wins
            TEST_CHECK(
                keeper.state().tap_counts[static_cast<std::size_t>(blaze4k::TapJudgment::Excellent)] == 1);
        }
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 1.0, blaze4k::NoteType::Tap));
            chart.notes.push_back(make_note(1, 1.0, blaze4k::NoteType::Tap));
            chart.tap_count = 2;
            blaze4k::ScoreKeeper keeper;
            keeper.reset(&chart, &k);
            keeper.consume(make_tap(0, 0, blaze4k::TapJudgment::Great, 0.0));
            keeper.consume(make_miss(1, 1));
            TEST_CHECK(keeper.actual_dance_points() == -12); // row is a miss
            TEST_CHECK(keeper.state().tap_counts[static_cast<std::size_t>(blaze4k::TapJudgment::Miss)] == 1);
            TEST_CHECK(keeper.state().combo == 0);
            TEST_CHECK(keeper.state().miss_combo == 1);
        }
        std::cout << "  - Row score uses the latest hit; any miss dominates.\n";
    }

    // 6. Combo rules.
    {
        // Three separate Great taps: 1, 2, 3.
        blaze4k::Chart chart;
        for (int i = 0; i < 3; ++i) {
            chart.notes.push_back(make_note(i, static_cast<double>(i), blaze4k::NoteType::Tap));
        }
        chart.tap_count = 3;
        blaze4k::ScoreKeeper keeper;
        keeper.reset(&chart, &k);
        keeper.consume(make_tap(0, 0, blaze4k::TapJudgment::Great, 0.0));
        TEST_CHECK(keeper.state().combo == 1);
        keeper.consume(make_tap(1, 1, blaze4k::TapJudgment::Great, 0.0));
        TEST_CHECK(keeper.state().combo == 2);
        keeper.consume(make_tap(2, 2, blaze4k::TapJudgment::Great, 0.0));
        TEST_CHECK(keeper.state().combo == 3);
        TEST_CHECK(keeper.state().max_combo == 3);
    }
    {
        // Decent breaks combo; a later Great starts a new one.
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(0, 0.0, blaze4k::NoteType::Tap));
        chart.notes.push_back(make_note(0, 1.0, blaze4k::NoteType::Tap));
        chart.notes.push_back(make_note(0, 2.0, blaze4k::NoteType::Tap));
        chart.tap_count = 3;
        blaze4k::ScoreKeeper keeper;
        keeper.reset(&chart, &k);
        keeper.consume(make_tap(0, 0, blaze4k::TapJudgment::Great, 0.0));
        keeper.consume(make_tap(1, 0, blaze4k::TapJudgment::Decent, 0.0));
        TEST_CHECK(keeper.state().combo == 0);
        keeper.consume(make_tap(2, 0, blaze4k::TapJudgment::Great, 0.0));
        TEST_CHECK(keeper.state().combo == 1);
    }
    {
        // Miss sets miss_combo; a following Great clears it.
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(0, 0.0, blaze4k::NoteType::Tap));
        chart.notes.push_back(make_note(0, 1.0, blaze4k::NoteType::Tap));
        chart.notes.push_back(make_note(0, 2.0, blaze4k::NoteType::Tap));
        chart.tap_count = 3;
        blaze4k::ScoreKeeper keeper;
        keeper.reset(&chart, &k);
        keeper.consume(make_tap(0, 0, blaze4k::TapJudgment::Great, 0.0));
        keeper.consume(make_miss(1, 0));
        TEST_CHECK(keeper.state().combo == 0);
        TEST_CHECK(keeper.state().miss_combo == 1);
        keeper.consume(make_tap(2, 0, blaze4k::TapJudgment::Great, 0.0));
        TEST_CHECK(keeper.state().combo == 1);
        TEST_CHECK(keeper.state().miss_combo == 0);
    }
    {
        // Hold outcomes and hit mines never change combo.
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(0, 0.0, blaze4k::NoteType::Tap));
        chart.notes.push_back(make_note(1, 1.0, blaze4k::NoteType::HoldHead, 2.0));
        chart.notes.push_back(make_note(2, 3.0, blaze4k::NoteType::Mine));
        chart.tap_count = 1;
        chart.hold_count = 1;
        chart.mine_count = 1;
        blaze4k::ScoreKeeper keeper;
        keeper.reset(&chart, &k);
        keeper.consume(make_tap(0, 0, blaze4k::TapJudgment::Great, 0.0));
        keeper.consume(make_tap(1, 1, blaze4k::TapJudgment::Fantastic, 0.0));
        TEST_CHECK(keeper.state().combo == 2);

        keeper.consume(make_hold_outcome(1, 1, blaze4k::NoteType::HoldHead, blaze4k::JudgmentKind::HoldNg,
                                         blaze4k::HoldJudgment::Ng));
        TEST_CHECK(keeper.state().combo == 2);

        blaze4k::JudgmentEvent mine;
        mine.kind = blaze4k::JudgmentKind::HitMine;
        mine.column = 2;
        mine.note_index = 2;
        mine.note_type = blaze4k::NoteType::Mine;
        mine.window = blaze4k::TapJudgment::HitMine;
        keeper.consume(mine);
        TEST_CHECK(keeper.state().combo == 2);
    }
    std::cout << "  - Combo continues on Great+, breaks on Decent/worse, ignores holds/mines.\n";

    // 7. Hold DP.
    {
        struct Case {
            blaze4k::TapJudgment head;
            blaze4k::HoldJudgment outcome;
            bool has_outcome;
            int expected_dp;
            double expected_percent;
        };
        const Case cases[] = {
            {blaze4k::TapJudgment::Fantastic, blaze4k::HoldJudgment::Ok, true, 10, 1.0},
            {blaze4k::TapJudgment::Great, blaze4k::HoldJudgment::Ok, true, 7, 0.7},
            {blaze4k::TapJudgment::Fantastic, blaze4k::HoldJudgment::Ng, true, 5, 0.5},
            {blaze4k::TapJudgment::Miss, blaze4k::HoldJudgment::Num, false, -12, -1.2},
        };
        for (const Case& c : cases) {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 1.0, blaze4k::NoteType::HoldHead, 2.0));
            chart.hold_count = 1;
            blaze4k::ScoreKeeper keeper;
            keeper.reset(&chart, &k);
            TEST_CHECK(keeper.possible_dance_points() == 10);

            if (c.head == blaze4k::TapJudgment::Miss) {
                blaze4k::JudgmentEvent miss;
                miss.kind = blaze4k::JudgmentKind::Miss;
                miss.column = 0;
                miss.note_index = 0;
                miss.note_type = blaze4k::NoteType::HoldHead;
                miss.window = blaze4k::TapJudgment::Miss;
                keeper.consume(miss);
            } else {
                blaze4k::JudgmentEvent head;
                head.kind = blaze4k::JudgmentKind::Tap;
                head.column = 0;
                head.note_index = 0;
                head.note_type = blaze4k::NoteType::HoldHead;
                head.window = c.head;
                keeper.consume(head);
                if (c.has_outcome) {
                    keeper.consume(make_hold_outcome(
                        0, 0, blaze4k::NoteType::HoldHead,
                        c.outcome == blaze4k::HoldJudgment::Ok ? blaze4k::JudgmentKind::HoldOk
                                                          : blaze4k::JudgmentKind::HoldNg,
                        c.outcome));
                }
            }
            TEST_CHECK(keeper.actual_dance_points() == c.expected_dp);
            TEST_CHECK(approx(keeper.percent(), c.expected_percent));
        }
        std::cout << "  - Hold head DP plus OK/NG outcome matches OpenITG.\n";
    }

    // 8. Roll DP; RollHit events are score-neutral.
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(2, 1.0, blaze4k::NoteType::RollHead, 2.0));
        chart.roll_count = 1;
        blaze4k::ScoreKeeper keeper;
        keeper.reset(&chart, &k);
        TEST_CHECK(keeper.possible_dance_points() == 10);

        blaze4k::JudgmentEvent head;
        head.kind = blaze4k::JudgmentKind::Tap;
        head.column = 2;
        head.note_index = 0;
        head.note_type = blaze4k::NoteType::RollHead;
        head.window = blaze4k::TapJudgment::Fantastic;
        keeper.consume(head);

        blaze4k::JudgmentEvent hit;
        hit.kind = blaze4k::JudgmentKind::RollHit;
        hit.column = 2;
        hit.note_index = 0;
        hit.note_type = blaze4k::NoteType::RollHead;
        keeper.consume(hit);
        TEST_CHECK(keeper.actual_dance_points() == 5); // re-hit adds nothing

        keeper.consume(make_hold_outcome(0, 2, blaze4k::NoteType::RollHead, blaze4k::JudgmentKind::RollOk,
                                         blaze4k::HoldJudgment::Ok));
        TEST_CHECK(keeper.actual_dance_points() == 10);
        TEST_CHECK(approx(keeper.percent(), 1.0));
        std::cout << "  - Roll scoring matches holds; re-hits are neutral.\n";
    }

    // 9. Mines: hit mine penalizes without combo change; avoided is neutral.
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(0, 0.0, blaze4k::NoteType::Tap));
        chart.notes.push_back(make_note(1, 1.0, blaze4k::NoteType::Mine));
        chart.tap_count = 1;
        chart.mine_count = 1;
        blaze4k::ScoreKeeper keeper;
        keeper.reset(&chart, &k);
        TEST_CHECK(keeper.possible_dance_points() == 5);

        keeper.consume(make_tap(0, 0, blaze4k::TapJudgment::Fantastic, 0.0));
        TEST_CHECK(keeper.state().combo == 1);

        blaze4k::JudgmentEvent hit_mine;
        hit_mine.kind = blaze4k::JudgmentKind::HitMine;
        hit_mine.column = 1;
        hit_mine.note_index = 1;
        hit_mine.note_type = blaze4k::NoteType::Mine;
        hit_mine.window = blaze4k::TapJudgment::HitMine;
        keeper.consume(hit_mine);
        TEST_CHECK(keeper.actual_dance_points() == -1); // 5 + (-6)
        TEST_CHECK(keeper.state().combo == 1);
        TEST_CHECK(keeper.state().tap_counts[static_cast<std::size_t>(blaze4k::TapJudgment::HitMine)] == 1);

        blaze4k::JudgmentEvent avoided;
        avoided.kind = blaze4k::JudgmentKind::AvoidedMine;
        avoided.column = 1;
        avoided.note_index = 1;
        avoided.note_type = blaze4k::NoteType::Mine;
        keeper.consume(avoided);
        TEST_CHECK(keeper.actual_dance_points() == -1);
        std::cout << "  - Hit mine scores -6 without touching combo; avoided is neutral.\n";
    }

    // 10. Percent edge cases.
    {
        blaze4k::ScoreKeeper keeper;
        keeper.reset(nullptr, &k);
        keeper.consume(make_tap(0, 0, blaze4k::TapJudgment::Fantastic, 0.0));
        TEST_CHECK(keeper.percent() == 0.0);
        TEST_CHECK(keeper.possible_dance_points() == 0);

        blaze4k::Chart empty;
        keeper.reset(&empty, &k);
        TEST_CHECK(keeper.percent() == 0.0);

        blaze4k::Chart chart;
        chart.notes.push_back(make_note(0, 0.0, blaze4k::NoteType::Tap));
        chart.tap_count = 1;
        keeper.reset(&chart, &k);
        keeper.consume(make_tap(0, 0, blaze4k::TapJudgment::Fantastic, 0.0));
        TEST_CHECK(keeper.percent() == 1.0); // actual == possible rounding correction
        std::cout << "  - Empty/null chart is safe; equal DP yields exactly 1.0.\n";
    }

    // 11. Idempotence: duplicates never double-count.
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(0, 0.0, blaze4k::NoteType::Tap));
        chart.notes.push_back(make_note(1, 1.0, blaze4k::NoteType::HoldHead, 2.0));
        chart.tap_count = 1;
        chart.hold_count = 1;
        blaze4k::ScoreKeeper keeper;
        keeper.reset(&chart, &k);
        keeper.consume(make_tap(0, 0, blaze4k::TapJudgment::Fantastic, 0.0));
        keeper.consume(make_tap(0, 0, blaze4k::TapJudgment::Fantastic, 0.0));
        TEST_CHECK(keeper.actual_dance_points() == 5);
        TEST_CHECK(keeper.state().combo == 1);

        keeper.consume(make_hold_outcome(1, 1, blaze4k::NoteType::HoldHead, blaze4k::JudgmentKind::HoldOk,
                                         blaze4k::HoldJudgment::Ok));
        keeper.consume(make_hold_outcome(1, 1, blaze4k::NoteType::HoldHead, blaze4k::JudgmentKind::HoldOk,
                                         blaze4k::HoldJudgment::Ok));
        TEST_CHECK(keeper.actual_dance_points() == 10);

        // Idempotence is per note, not per row: a duplicate on one note of a
        // multi-note row must not resolve the row early and swallow another note.
        blaze4k::Chart jump;
        jump.notes.push_back(make_note(0, 0.0, blaze4k::NoteType::Tap));
        jump.notes.push_back(make_note(1, 0.0, blaze4k::NoteType::Tap));
        jump.tap_count = 2;
        blaze4k::ScoreKeeper jump_keeper;
        jump_keeper.reset(&jump, &k);
        jump_keeper.consume(make_tap(0, 0, blaze4k::TapJudgment::Fantastic, 0.0));
        jump_keeper.consume(make_tap(0, 0, blaze4k::TapJudgment::Fantastic, 0.0)); // duplicate note A
        jump_keeper.consume(make_miss(1, 1));                                 // real note B
        TEST_CHECK(jump_keeper.actual_dance_points() == -12);
        TEST_CHECK(
            jump_keeper.state().tap_counts[static_cast<std::size_t>(blaze4k::TapJudgment::Miss)] == 1);
        TEST_CHECK(jump_keeper.state().combo == 0);
        TEST_CHECK(jump_keeper.state().miss_combo == 1);
        TEST_CHECK(jump_keeper.is_complete());
        std::cout << "  - Duplicate events are ignored (per-note/hold guards).\n";
    }

    // 12. Incremental == batch.
    {
        blaze4k::Chart chart;
        for (int i = 0; i < 4; ++i) {
            chart.notes.push_back(make_note(i % 4, static_cast<double>(i), blaze4k::NoteType::Tap));
        }
        chart.tap_count = 4;
        std::vector<blaze4k::JudgmentEvent> events = {
            make_tap(0, 0, blaze4k::TapJudgment::Fantastic, 0.0),
            make_tap(1, 1, blaze4k::TapJudgment::Great, -20.0),
            make_tap(2, 2, blaze4k::TapJudgment::Decent, 0.0),
            make_miss(3, 3),
        };

        blaze4k::ScoreKeeper incremental;
        incremental.reset(&chart, &k);
        for (const blaze4k::JudgmentEvent& event : events) {
            incremental.consume(event);
        }

        blaze4k::ScoreKeeper batch;
        batch.reset(&chart, &k);
        batch.consume(events);

        TEST_CHECK(incremental.actual_dance_points() == batch.actual_dance_points());
        TEST_CHECK(incremental.state().combo == batch.state().combo);
        TEST_CHECK(incremental.state().max_combo == batch.state().max_combo);
        TEST_CHECK(incremental.state().miss_combo == batch.state().miss_combo);
        TEST_CHECK(approx(incremental.percent(), batch.percent()));
        for (std::size_t i = 0; i < incremental.state().tap_counts.size(); ++i) {
            TEST_CHECK(incremental.state().tap_counts[i] == batch.state().tap_counts[i]);
        }
        std::cout << "  - Incremental and batch consumption agree.\n";
    }

    // 13. Engine integration on a reference chart (hand-computed OpenITG arithmetic):
    //     tap col0 Fantastic            -> +5
    //     tap col1 expires (Miss)       -> -12
    //     hold head col2 Fantastic      -> +5
    //     hold col2 held through end Ok -> +5
    //   possible = 3 rows*5 + 1 hold*5 = 20; actual = 3; percent = 0.15; grade D.
    //   combo: 1 -> 0 (miss) -> 1 (head); max 1. Counts: Fantastic 2, Miss 1, OK 1.
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(0, 1.0, blaze4k::NoteType::Tap));
        chart.notes.push_back(make_note(1, 2.0, blaze4k::NoteType::Tap));
        chart.notes.push_back(make_note(2, 3.0, blaze4k::NoteType::HoldHead, 5.0));
        chart.tap_count = 2;
        chart.hold_count = 1;

        blaze4k::JudgmentEngine engine;
        engine.reset(&chart, &k);
        blaze4k::ScoreKeeper keeper;
        keeper.reset(&chart, &k);

        std::vector<blaze4k::JudgmentEvent> drained;

        engine.handle_step(0, 1.0);
        engine.update(2.5, held_none()); // col1 tap expires as Miss
        engine.handle_step(2, 3.0);      // hold head Fantastic
        engine.update(5.0, held_col(2)); // hold OK

        engine.drain_new_events(drained);
        keeper.consume(drained);

        TEST_CHECK(keeper.actual_dance_points() == 3);
        TEST_CHECK(keeper.possible_dance_points() == 20);
        TEST_CHECK(approx(keeper.percent(), 0.15));
        TEST_CHECK(std::string(keeper.grade().label) == "D");
        TEST_CHECK(keeper.state().combo == 1);
        TEST_CHECK(keeper.state().max_combo == 1);
        TEST_CHECK(keeper.state().tap_counts[static_cast<std::size_t>(blaze4k::TapJudgment::Fantastic)] == 2);
        TEST_CHECK(keeper.state().tap_counts[static_cast<std::size_t>(blaze4k::TapJudgment::Miss)] == 1);
        TEST_CHECK(keeper.state().hold_counts[static_cast<std::size_t>(blaze4k::HoldJudgment::Ok)] == 1);
        TEST_CHECK(keeper.is_complete());
        std::cout << "  - Keeper derives DP/percent/combo/counts from the B4 log.\n";
    }

    // 14. HUD formatting.
    {
        TEST_CHECK(blaze4k::format_percent(1.0) == "100.00%");
        TEST_CHECK(blaze4k::format_percent(0.9745) == "97.45%");
        TEST_CHECK(blaze4k::format_percent(0.99999) == "99.99%"); // truncation, not rounding
        TEST_CHECK(blaze4k::format_percent(-0.5) == "0.00%");     // display clamp
        TEST_CHECK(blaze4k::format_combo(0) == "0");
        TEST_CHECK(blaze4k::format_combo(123) == "123");

        blaze4k::GradeTier quad{1.0, "quad_star"};
        blaze4k::GradeTier triple{0.99, "triple_star"};
        blaze4k::GradeTier single{0.96, "single_star"};
        blaze4k::GradeTier s_plus{0.94, "S+"};
        TEST_CHECK(blaze4k::format_grade(quad) == "****");
        TEST_CHECK(blaze4k::format_grade(triple) == "***");
        TEST_CHECK(blaze4k::format_grade(single) == "*");
        TEST_CHECK(blaze4k::format_grade(s_plus) == "S+");
        std::cout << "  - Percent truncation/display-clamp and grade labels are correct.\n";
    }

    // 15. GameplayView boundary: the live update drains the B4 log into the keeper.
    //     The stub clock advances deterministically with `fixed_dt`, so an untouched
    //     tap must expire as a Miss and surface through `score_state()`.
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(0, 1.0, blaze4k::NoteType::Tap));
        chart.tap_count = 1;

        blaze4k::GameplayView view;
        blaze4k::GameplayOptions options;
        TEST_CHECK(view.init(chart, k, "", options));
        TEST_CHECK(view.is_ready());

        for (int i = 0; i < 90; ++i) { // ~1.5 s of stub time, past the Miss threshold
            view.update(1.0 / 60.0, held_none());
        }

        TEST_CHECK(view.dance_points() == -12);
        TEST_CHECK(view.score_state().possible_dp == 5);
        TEST_CHECK(approx(view.score_percent(), -2.4));
        TEST_CHECK(view.score_state().tap_counts[static_cast<std::size_t>(blaze4k::TapJudgment::Miss)] == 1);
        view.shutdown();
        std::cout << "  - GameplayView drives scoring from the drained judgment log.\n";
    }

    // 16. AC2/AC3 end-to-end: perfect play over a parsed reference-pack chart
    //     must yield exactly the possible DP (100%, quad star).
    {
        std::filesystem::path ref =
            "tests/fixtures/reference_pack/Blaze Pack/Blaze Anthem/Blaze Anthem.sm";
        if (!std::filesystem::exists(ref)) {
            ref = "../tests/fixtures/reference_pack/Blaze Pack/Blaze Anthem/Blaze Anthem.sm";
        }
        if (!std::filesystem::exists(ref)) {
            ref = "../../tests/fixtures/reference_pack/Blaze Pack/Blaze Anthem/Blaze Anthem.sm";
        }
        TEST_CHECK(std::filesystem::exists(ref));

        blaze4k::SimfileParser parser;
        TEST_CHECK(parser.parse_file(ref.string()));
        TEST_CHECK(!parser.charts().empty());
        const blaze4k::Chart& chart = parser.charts()[0];

        blaze4k::ScoreKeeper keeper;
        keeper.reset(&chart, &k);

        for (std::size_t i = 0; i < chart.notes.size(); ++i) {
            const blaze4k::Note& note = chart.notes[i];
            if (note.type == blaze4k::NoteType::Mine) {
                continue;
            }
            const int index = static_cast<int>(i);
            keeper.consume(make_tap(index, note.column, blaze4k::TapJudgment::Fantastic, 0.0));
            if (note.type == blaze4k::NoteType::HoldHead) {
                keeper.consume(make_hold_outcome(index, note.column, note.type,
                                                 blaze4k::JudgmentKind::HoldOk, blaze4k::HoldJudgment::Ok));
            } else if (note.type == blaze4k::NoteType::RollHead) {
                keeper.consume(make_hold_outcome(index, note.column, note.type,
                                                 blaze4k::JudgmentKind::RollOk, blaze4k::HoldJudgment::Ok));
            }
        }

        TEST_CHECK(keeper.is_complete());
        TEST_CHECK(keeper.actual_dance_points() == keeper.possible_dance_points());
        TEST_CHECK(approx(keeper.percent(), 1.0));
        TEST_CHECK(std::string(keeper.grade().label) == "quad_star");
        std::cout << "  - Perfect play over a parsed reference chart yields 100% quad star.\n";
    }

    std::cout << "[score_keeper_test] All scoring tests passed successfully!\n";
    return 0;
}
