#include <array>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#include "chart/chart.hpp"
#include "gameplay/judgment.hpp"
#include "gameplay/judgment_engine.hpp"
#include "gameplay/judgment_input.hpp"
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

std::array<bool, 4> held_none() {
    return std::array<bool, 4>{false, false, false, false};
}

std::array<bool, 4> held_col(int column) {
    std::array<bool, 4> held{false, false, false, false};
    held[static_cast<std::size_t>(column)] = true;
    return held;
}

const blaze4k::JudgmentConstants& constants() {
    return blaze4k::JudgmentConstants::compiled_defaults();
}

} // namespace

int main() {
    std::cout << "[judgment_engine_test] Starting judgment engine tests...\n";
    const blaze4k::JudgmentConstants& k = constants();
    // The windows the engine actually applies (base * scale + add, OpenITG
    // AdjustedWindowTap/Hold, Player.cpp:34-74). pad_stick is read raw from k.
    const blaze4k::TimingWindows w = k.effective_windows();

    // 1. Tap classification + exact event fields. Each case uses a fresh note.
    {
        const double note_time = 2.0;
        struct Case {
            double delta;
            blaze4k::TapJudgment expected;
        };
        const Case cases[] = {
            {0.0, blaze4k::TapJudgment::Fantastic},
            {(w.fantastic + w.excellent) / 2.0, blaze4k::TapJudgment::Excellent},
            {(w.excellent + w.great) / 2.0, blaze4k::TapJudgment::Great},
            {(w.great + w.decent) / 2.0, blaze4k::TapJudgment::Decent},
            {(w.decent + w.way_off) / 2.0, blaze4k::TapJudgment::WayOff},
        };
        for (const Case& c : cases) {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(1, note_time, blaze4k::NoteType::Tap));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(1, note_time + c.delta);

            TEST_CHECK(engine.events().size() == 1);
            const blaze4k::JudgmentEvent& e = engine.events().front();
            TEST_CHECK(e.kind == blaze4k::JudgmentKind::Tap);
            TEST_CHECK(e.column == 1);
            TEST_CHECK(approx(e.note_time_seconds, note_time));
            TEST_CHECK(approx(e.hit_time_seconds, note_time + c.delta));
            TEST_CHECK(approx(e.delta_ms, c.delta * 1000.0, 1e-6));
            TEST_CHECK(e.window == c.expected);
            TEST_CHECK(e.note_type == blaze4k::NoteType::Tap);
            TEST_CHECK(e.note_index == 0);
        }

        // A step beyond Way Off yields TNS_NONE: no event at all.
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(1, note_time, blaze4k::NoteType::Tap));
        blaze4k::JudgmentEngine engine;
        engine.reset(&chart, &k);
        engine.handle_step(1, note_time + w.way_off + 0.01);
        TEST_CHECK(engine.events().empty());
        std::cout << "  - Tap windows classify and emit exact event fields.\n";
    }

    // 2. Delta sign: negative = early, positive = late.
    {
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 2.0, blaze4k::NoteType::Tap));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(0, 1.95);
            TEST_CHECK(engine.events().size() == 1);
            TEST_CHECK(engine.events().front().delta_ms < 0.0);
        }
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 2.0, blaze4k::NoteType::Tap));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(0, 2.05);
            TEST_CHECK(engine.events().size() == 1);
            TEST_CHECK(engine.events().front().delta_ms > 0.0);
        }
        std::cout << "  - Delta sign matches OpenITG (negative = early).\n";
    }

    // 3. Miss expiry (deterministic, per-note) and no Miss after a hit.
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(0, 2.0, blaze4k::NoteType::Tap));
        blaze4k::JudgmentEngine engine;
        engine.reset(&chart, &k);

        engine.update(2.0 + w.way_off - 0.001, held_none()); // just before threshold
        TEST_CHECK(engine.events().empty());

        engine.update(2.0 + w.way_off + 1e-3, held_none());
        TEST_CHECK(engine.events().size() == 1);
        const blaze4k::JudgmentEvent& e = engine.events().front();
        TEST_CHECK(e.kind == blaze4k::JudgmentKind::Miss);
        TEST_CHECK(e.window == blaze4k::TapJudgment::Miss);
        TEST_CHECK(approx(e.hit_time_seconds, 2.0 + w.way_off));
        TEST_CHECK(approx(e.delta_ms, w.way_off * 1000.0, 1e-6));

        blaze4k::Chart chart2;
        chart2.notes.push_back(make_note(0, 2.0, blaze4k::NoteType::Tap));
        blaze4k::JudgmentEngine engine2;
        engine2.reset(&chart2, &k);
        engine2.handle_step(0, 2.0);
        engine2.update(3.0, held_none());
        TEST_CHECK(engine2.events().size() == 1);
        TEST_CHECK(engine2.events().front().kind == blaze4k::JudgmentKind::Tap);
        std::cout << "  - Untouched tap expires as Miss; hit tap never misses.\n";
    }

    // 4. Mines: in-window hit, out-of-window none, untouched avoided (never Miss).
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(2, 2.0, blaze4k::NoteType::Mine));
        blaze4k::JudgmentEngine engine;
        engine.reset(&chart, &k);
        engine.handle_step(2, 2.0 + w.hit_mine - 0.01);
        TEST_CHECK(engine.events().size() == 1);
        TEST_CHECK(engine.events().front().kind == blaze4k::JudgmentKind::HitMine);
        TEST_CHECK(engine.events().front().window == blaze4k::TapJudgment::HitMine);
    }
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(2, 2.0, blaze4k::NoteType::Mine));
        blaze4k::JudgmentEngine engine;
        engine.reset(&chart, &k);
        engine.handle_step(2, 2.0 + w.hit_mine + 0.01);
        TEST_CHECK(engine.events().empty());
    }
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(2, 2.0, blaze4k::NoteType::Mine));
        blaze4k::JudgmentEngine engine;
        engine.reset(&chart, &k);
        engine.update(2.0 + w.way_off + 1e-3, held_none());
        TEST_CHECK(engine.events().size() == 1);
        TEST_CHECK(engine.events().front().kind == blaze4k::JudgmentKind::AvoidedMine);
        TEST_CHECK(engine.events().front().kind != blaze4k::JudgmentKind::Miss);
        std::cout << "  - Mine hit/avoid semantics match the pinned window.\n";
    }

    // 5. Hold OK: head hit at Great, held through the end.
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(0, 2.0, blaze4k::NoteType::HoldHead, 4.0));
        blaze4k::JudgmentEngine engine;
        engine.reset(&chart, &k);
        engine.handle_step(0, 2.08);
        TEST_CHECK(engine.events().size() == 1);
        TEST_CHECK(engine.events().front().window == blaze4k::TapJudgment::Great);

        engine.update(4.0, held_col(0));
        TEST_CHECK(engine.events().size() == 2);
        const blaze4k::JudgmentEvent& hold = engine.events().back();
        TEST_CHECK(hold.kind == blaze4k::JudgmentKind::HoldOk);
        TEST_CHECK(hold.hold == blaze4k::HoldJudgment::Ok);
        std::cout << "  - Hold held through the end scores HoldOk.\n";
    }

    // 6. Hold NG: head hit, then released beyond the OK window.
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(0, 2.0, blaze4k::NoteType::HoldHead, 4.0));
        blaze4k::JudgmentEngine engine;
        engine.reset(&chart, &k);
        engine.handle_step(0, 2.0);
        engine.update(2.0 + w.hold_ok + 0.01, held_none());
        TEST_CHECK(engine.events().size() == 2);
        TEST_CHECK(engine.events().back().kind == blaze4k::JudgmentKind::HoldNg);
        TEST_CHECK(engine.events().back().hold == blaze4k::HoldJudgment::Ng);

        // An early head hit starts the hold clock at the note row, not at the
        // early hit time: no life is lost before the hold begins.
        blaze4k::Chart early_chart;
        early_chart.notes.push_back(make_note(0, 2.0, blaze4k::NoteType::HoldHead, 4.0));
        blaze4k::JudgmentEngine early_engine;
        early_engine.reset(&early_chart, &k);
        early_engine.handle_step(0, 1.9); // early head hit
        early_engine.update(2.31, held_none());
        TEST_CHECK(early_engine.events().size() == 1); // still pending, not NG
        early_engine.update(2.33, held_none());
        TEST_CHECK(early_engine.events().back().kind == blaze4k::JudgmentKind::HoldNg);
        std::cout << "  - Hold released past the OK window scores HoldNg.\n";
    }

    // 7. Missed hold head: only a Miss event, never an OK/NG.
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(0, 2.0, blaze4k::NoteType::HoldHead, 4.0));
        blaze4k::JudgmentEngine engine;
        engine.reset(&chart, &k);
        engine.update(2.0 + w.way_off + 1e-3, held_none());
        TEST_CHECK(engine.events().size() == 1);
        TEST_CHECK(engine.events().front().kind == blaze4k::JudgmentKind::Miss);

        engine.update(4.5, held_col(0));
        TEST_CHECK(engine.events().size() == 1);
        std::cout << "  - Missed hold head never produces HoldOk/HoldNg.\n";
    }

    // 7b. An active hold keeps its body after the head is judged away; once the
    // hold resolves (OK/NG) or its head is missed, it is no longer in progress.
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(0, 2.0, blaze4k::NoteType::HoldHead, 4.0));
        blaze4k::JudgmentEngine engine;
        engine.reset(&chart, &k);

        TEST_CHECK(!engine.is_hold_in_progress(0)); // head not hit yet
        engine.handle_step(0, 2.0);                 // Fantastic head
        TEST_CHECK(engine.is_note_hidden(0));       // head judged away
        TEST_CHECK(engine.is_hold_in_progress(0));  // body still scrolling
        engine.update(4.0, held_col(0));            // tail reached -> HoldOk
        TEST_CHECK(!engine.is_hold_in_progress(0)); // resolved

        blaze4k::Chart missed_chart;
        missed_chart.notes.push_back(make_note(0, 2.0, blaze4k::NoteType::HoldHead, 4.0));
        blaze4k::JudgmentEngine missed_engine;
        missed_engine.reset(&missed_chart, &k);
        missed_engine.update(2.0 + w.way_off + 1e-3, held_none());
        TEST_CHECK(!missed_engine.is_hold_in_progress(0));
        TEST_CHECK(!missed_engine.is_hold_head_hit(0));
        std::cout << "  - Active hold keeps its body after the head is hidden.\n";
    }

    // 7c. Presentation accessors: last-held time follows the button and freezes
    // on release; the outcome stays readable after the hold resolves.
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(1, 2.0, blaze4k::NoteType::HoldHead, 4.0));
        blaze4k::JudgmentEngine engine;
        engine.reset(&chart, &k);

        engine.handle_step(1, 2.0);
        TEST_CHECK(engine.is_hold_head_hit(0));
        TEST_CHECK(engine.hold_judgment(0) == blaze4k::HoldJudgment::Num);
        engine.update(2.5, held_col(1));
        TEST_CHECK(std::abs(engine.hold_last_held_seconds(0) - 2.5) < 1e-9);
        engine.update(2.6, held_none()); // let go: last-held time stays at 2.5
        TEST_CHECK(std::abs(engine.hold_last_held_seconds(0) - 2.5) < 1e-9);
        engine.update(2.5 + w.hold_ok + 1e-3, held_none());
        TEST_CHECK(engine.hold_judgment(0) == blaze4k::HoldJudgment::Ng);
        TEST_CHECK(engine.is_hold_head_hit(0));
        TEST_CHECK(engine.hold_judgment(-1) == blaze4k::HoldJudgment::Num);
        std::cout << "  - Hold last-held time and outcome accessors ok.\n";
    }

    // 8. Roll re-hits refresh life; end => RollOk; neglected => RollNg.
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(2, 2.0, blaze4k::NoteType::RollHead, 4.0));
        blaze4k::JudgmentEngine engine;
        engine.reset(&chart, &k);
        engine.handle_step(2, 2.0);  // head
        engine.handle_step(2, 2.5);  // re-hit
        engine.handle_step(2, 3.0);  // re-hit
        engine.handle_step(2, 3.9);  // re-hit keeps life up
        engine.update(4.0, held_none());

        int roll_hits = 0;
        for (const blaze4k::JudgmentEvent& e : engine.events()) {
            if (e.kind == blaze4k::JudgmentKind::RollHit) {
                ++roll_hits;
            }
        }
        TEST_CHECK(roll_hits == 3);
        TEST_CHECK(engine.events().back().kind == blaze4k::JudgmentKind::RollOk);
        TEST_CHECK(engine.events().back().hold == blaze4k::HoldJudgment::Ok);
    }
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(2, 2.0, blaze4k::NoteType::RollHead, 5.0));
        blaze4k::JudgmentEngine engine;
        engine.reset(&chart, &k);
        engine.handle_step(2, 2.0);
        engine.update(2.0 + w.hold_roll + 0.01, held_none());
        TEST_CHECK(engine.events().size() == 2);
        TEST_CHECK(engine.events().back().kind == blaze4k::JudgmentKind::RollNg);
        std::cout << "  - Roll re-hits refresh; neglected roll scores RollNg.\n";
    }

    // 9. Closest-note selection (never re-judges) and later-note tie-break.
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(3, 2.0, blaze4k::NoteType::Tap));
        chart.notes.push_back(make_note(3, 2.5, blaze4k::NoteType::Tap));
        blaze4k::JudgmentEngine engine;
        engine.reset(&chart, &k);

        engine.handle_step(3, 2.05);
        TEST_CHECK(engine.events().size() == 1);
        TEST_CHECK(engine.events().back().note_index == 0);

        engine.handle_step(3, 2.45);
        TEST_CHECK(engine.events().size() == 2);
        TEST_CHECK(engine.events().back().note_index == 1);

        engine.handle_step(3, 2.0);
        TEST_CHECK(engine.events().size() == 2); // both graded: no re-judge
    }
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(0, 2.0, blaze4k::NoteType::Tap));
        chart.notes.push_back(make_note(0, 2.125, blaze4k::NoteType::Tap));
        blaze4k::JudgmentEngine engine;
        engine.reset(&chart, &k);
        engine.handle_step(0, 2.0625); // exact binary tie; prefer the later note
        TEST_CHECK(engine.events().size() == 1);
        TEST_CHECK(engine.events().back().note_index == 1);
        std::cout << "  - Closest-note selection and tie-break correct.\n";
    }

    // 10. Held-over-mine crossing vs avoided at expiry.
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(1, 3.0, blaze4k::NoteType::Mine));
        blaze4k::JudgmentEngine engine;
        engine.reset(&chart, &k);
        engine.handle_step(1, 2.0); // no note in range: nothing
        TEST_CHECK(engine.events().empty());
        engine.update(3.06, held_col(1)); // crossing (pad_stick behind) while held
        TEST_CHECK(engine.events().size() == 1);
        TEST_CHECK(engine.events().front().kind == blaze4k::JudgmentKind::HitMine);
        TEST_CHECK(approx(engine.events().front().hit_time_seconds, 3.06 - k.windows.pad_stick));
    }
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(1, 3.0, blaze4k::NoteType::Mine));
        blaze4k::JudgmentEngine engine;
        engine.reset(&chart, &k);
        engine.update(3.05, held_none());
        TEST_CHECK(engine.events().empty());
        engine.update(3.0 + w.way_off + 1e-3, held_none());
        TEST_CHECK(engine.events().size() == 1);
        TEST_CHECK(engine.events().front().kind == blaze4k::JudgmentKind::AvoidedMine);
        std::cout << "  - Held-over-mine triggers; unheld mine is avoided.\n";
    }

    // 11. Frame-rate independence: coarse vs fine update grid, same judgment.
    {
        auto run = [&k](double step, double release_time, double hold_end) {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 2.0, blaze4k::NoteType::HoldHead, hold_end));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(0, 2.0);
            double t = 2.0;
            while (t < hold_end) {
                t = std::min(t + step, hold_end); // always visit the exact end time
                engine.update(t, t <= release_time ? held_col(0) : held_none());
            }
            return engine.events();
        };

        // Decay path: released at 2.2 (on both grids), so the analytic life at
        // the 2.5 tail is identical and both grids must score HoldOk.
        const std::vector<blaze4k::JudgmentEvent> coarse = run(0.1, 2.2, 2.5);
        const std::vector<blaze4k::JudgmentEvent> fine = run(0.01, 2.2, 2.5);
        TEST_CHECK(coarse.size() == 2 && fine.size() == 2);
        TEST_CHECK(coarse[0].kind == fine[0].kind);
        TEST_CHECK(coarse[1].kind == blaze4k::JudgmentKind::HoldOk);
        TEST_CHECK(fine[1].kind == blaze4k::JudgmentKind::HoldOk);
        TEST_CHECK(approx(coarse[1].hit_time_seconds, fine[1].hit_time_seconds));

        // Decayed to zero: released right after the head, both grids must score
        // HoldNg; the emission time follows the grid, the outcome does not.
        const std::vector<blaze4k::JudgmentEvent> coarse_ng = run(0.1, 2.0, 3.0);
        const std::vector<blaze4k::JudgmentEvent> fine_ng = run(0.01, 2.0, 3.0);
        TEST_CHECK(coarse_ng.size() == 2 && fine_ng.size() == 2);
        TEST_CHECK(coarse_ng[1].kind == blaze4k::JudgmentKind::HoldNg);
        TEST_CHECK(fine_ng[1].kind == blaze4k::JudgmentKind::HoldNg);
        TEST_CHECK(coarse_ng[1].note_index == fine_ng[1].note_index);

        // A single coarse stall that jumps past the tail while the button stays
        // down must not decay through the gap: still HoldOk.
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 2.0, blaze4k::NoteType::HoldHead, 2.5));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(0, 2.0);
            engine.update(2.6, held_col(0));
            TEST_CHECK(engine.events().size() == 2);
            TEST_CHECK(engine.events().back().kind == blaze4k::JudgmentKind::HoldOk);
            TEST_CHECK(engine.events().back().hold == blaze4k::HoldJudgment::Ok);
        }
        std::cout << "  - Hold life is frame-rate independent (analytic decay).\n";
    }

    // 12. Append-only log + drain semantics + latest_event.
    {
        blaze4k::Chart chart;
        chart.notes.push_back(make_note(0, 2.0, blaze4k::NoteType::Tap));
        chart.notes.push_back(make_note(0, 3.0, blaze4k::NoteType::Tap));
        blaze4k::JudgmentEngine engine;
        engine.reset(&chart, &k);
        TEST_CHECK(engine.latest_event() == nullptr);

        engine.handle_step(0, 2.0);
        TEST_CHECK(engine.latest_event() != nullptr);
        TEST_CHECK(engine.latest_event()->note_index == 0);

        std::vector<blaze4k::JudgmentEvent> first;
        engine.drain_new_events(first);
        TEST_CHECK(first.size() == 1);

        std::vector<blaze4k::JudgmentEvent> empty;
        engine.drain_new_events(empty);
        TEST_CHECK(empty.empty());

        engine.handle_step(0, 3.0);
        std::vector<blaze4k::JudgmentEvent> suffix;
        engine.drain_new_events(suffix);
        TEST_CHECK(suffix.size() == 1);
        TEST_CHECK(suffix.front().note_index == 1);
        TEST_CHECK(engine.events().size() == 2);
        std::cout << "  - Log is append-only with correct drain suffix.\n";
    }

    // 13. Visual/log consistency: hidden set matches the log exactly.
    {
        struct VisualCase {
            double delta;
            bool hidden;
        };
        const VisualCase cases[] = {
            {0.0, true},                       // Fantastic
            {0.03, true},                      // Excellent
            {0.08, true},                      // Great
            {0.12, false},                     // Decent
            {0.16, false},                     // Way Off
        };
        for (const VisualCase& c : cases) {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 2.0, blaze4k::NoteType::Tap));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(0, 2.0 + c.delta);
            TEST_CHECK(engine.is_note_judged(0));
            TEST_CHECK(engine.is_note_hidden(0) == c.hidden);
            if (c.hidden) {
                TEST_CHECK(!engine.events().empty());
                TEST_CHECK(engine.events().back().note_index == 0);
            }
        }

        // Expired (Miss) note is not hidden but is logged.
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 2.0, blaze4k::NoteType::Tap));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.update(2.0 + w.way_off + 1e-3, held_none());
            TEST_CHECK(!engine.is_note_hidden(0));
            TEST_CHECK(engine.events().back().kind == blaze4k::JudgmentKind::Miss);
        }

        // A hit mine is hidden.
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 2.0, blaze4k::NoteType::Mine));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(0, 2.0);
            TEST_CHECK(engine.is_note_hidden(0));
            TEST_CHECK(engine.events().back().kind == blaze4k::JudgmentKind::HitMine);
        }
        std::cout << "  - Hidden-note set matches the event log.\n";
    }

    // 14. music_time_for_event: pure SDL-ns -> music-time conversion.
    {
        TEST_CHECK(approx(blaze4k::music_time_for_event(1000, 1000, 5.0), 5.0));
        TEST_CHECK(approx(blaze4k::music_time_for_event(990000000ULL, 1000000000ULL, 5.0), 4.99));
        TEST_CHECK(approx(blaze4k::music_time_for_event(0, 1000, 5.0), 5.0));
        TEST_CHECK(approx(blaze4k::music_time_for_event(2000, 1000, 5.0), 5.0));
        // #81: aging reference prefers the clock's own estimate timestamp.
        TEST_CHECK(blaze4k::aging_reference_ns(0, 1234) == 1234);
        TEST_CHECK(blaze4k::aging_reference_ns(5678, 1234) == 5678);
        TEST_CHECK(approx(blaze4k::music_time_for_event(
                              997'000'000ULL, blaze4k::aging_reference_ns(1'000'000'000ULL, 2'000'000'000ULL),
                              5.0),
                          4.997));
        std::cout << "  - Input timestamp conversion is pure and correct.\n";
    }

    // 15. Empty/null chart: safe no-ops.
    {
        blaze4k::JudgmentEngine engine;
        engine.reset(nullptr, &k);
        engine.handle_step(0, 1.0);
        engine.update(1.0, held_col(0));
        TEST_CHECK(engine.events().empty());
        TEST_CHECK(engine.latest_event() == nullptr);
        TEST_CHECK(!engine.is_note_judged(0));
        TEST_CHECK(!engine.is_note_hidden(0));

        blaze4k::Chart empty;
        engine.reset(&empty, &k);
        engine.handle_step(0, 1.0);
        engine.update(2.0, held_none());
        TEST_CHECK(engine.events().empty());
        std::cout << "  - Null/empty chart is handled safely.\n";
    }

    // 16. Mine semantics (#56, OpenITG Player::Step / CrossedMineRow).
    {
        using blaze4k::JudgmentKind;
        using blaze4k::NoteType;
        const double P = k.windows.pad_stick;
        auto count_kind = [](const blaze4k::JudgmentEngine& engine, JudgmentKind kind) {
            return std::count_if(engine.events().begin(), engine.events().end(),
                                 [kind](const blaze4k::JudgmentEvent& e) { return e.kind == kind; });
        };
        const double expire_after = 2.0 + w.way_off + 1e-3;

        // 16.1 Step on a mine inside the window explodes it; outside it is consumed.
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 2.0, NoteType::Mine));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(0, 2.0 - 0.06);
            TEST_CHECK(engine.events().size() == 1);
            TEST_CHECK(engine.events().front().kind == JudgmentKind::HitMine);
            TEST_CHECK(approx(engine.events().front().delta_ms, -60.0, 1e-6));

            blaze4k::JudgmentEngine fresh;
            fresh.reset(&chart, &k);
            fresh.handle_step(0, 2.0 + w.hit_mine + 0.001); // just outside the effective mine window
            TEST_CHECK(fresh.events().empty());
        }

        // 16.2 The regression: a mine crossed unheld must not explode when the
        // player then steps on the next arrow in the same column.
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 2.0, NoteType::Mine));
            chart.notes.push_back(make_note(0, 2.125, NoteType::Tap));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.update(1.9, held_none());
            engine.update(2.06, held_none()); // mine crossed while unheld
            engine.handle_step(0, 2.10);
            TEST_CHECK(engine.events().size() == 1);
            TEST_CHECK(engine.events().back().kind == JudgmentKind::Tap);
            TEST_CHECK(engine.events().back().note_index == 1);
            TEST_CHECK(engine.events().back().window == blaze4k::TapJudgment::Excellent);
            engine.update(2.11, held_col(0));
            engine.update(2.2, held_col(0));
            engine.update(expire_after + 0.1, held_none());
            TEST_CHECK(count_kind(engine, JudgmentKind::HitMine) == 0);
            TEST_CHECK(count_kind(engine, JudgmentKind::AvoidedMine) == 1);
        }

        // 16.3 Tap next to a mine (mine after the tap): stepping the tap is safe.
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 2.0, NoteType::Tap));
            chart.notes.push_back(make_note(0, 2.125, NoteType::Mine));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(0, 2.03);
            TEST_CHECK(engine.events().size() == 1);
            TEST_CHECK(engine.events().back().kind == JudgmentKind::Tap);
            TEST_CHECK(engine.events().back().note_index == 0);
            engine.update(2.2, held_none());
            TEST_CHECK(count_kind(engine, JudgmentKind::HitMine) == 0);
        }

        // 16.4 A mine closer than the tap wins the step (OpenITG GetClosestNote,
        // mines included). Documents the rule; the tap later expires as Miss.
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 2.0, NoteType::Tap));
            chart.notes.push_back(make_note(0, 2.125, NoteType::Mine));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(0, 2.08);
            TEST_CHECK(engine.events().size() == 1);
            TEST_CHECK(engine.events().back().kind == JudgmentKind::HitMine);
            TEST_CHECK(engine.events().back().note_index == 1);
            TEST_CHECK(!engine.is_note_judged(0));
            engine.update(expire_after, held_none());
            TEST_CHECK(count_kind(engine, JudgmentKind::Miss) == 1);
            TEST_CHECK(engine.events().back().note_index == 0);
        }

        // 16.5 Pad-stick: a press after the crossing instant does not count as held.
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 2.0, NoteType::Mine));
            chart.notes.push_back(make_note(0, 2.1, NoteType::Tap));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.update(1.9, held_none());
            engine.handle_step(0, 2.08);
            TEST_CHECK(engine.events().size() == 1);
            TEST_CHECK(engine.events().back().kind == JudgmentKind::Tap);
            TEST_CHECK(engine.events().back().window == blaze4k::TapJudgment::Fantastic);
            engine.update(2.09, held_col(0)); // cursor 2.04 crosses; press 2.08 > 2.04
            engine.update(2.3, held_col(0));
            TEST_CHECK(count_kind(engine, JudgmentKind::HitMine) == 0);
            TEST_CHECK(count_kind(engine, JudgmentKind::AvoidedMine) == 1);
        }

        // 16.6 Pad-stick: released within pad_stick after the mine's time.
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 2.0, NoteType::Mine));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(0, 1.8); // mine 0.2 s away: nothing
            TEST_CHECK(engine.events().empty());
            engine.update(2.02, held_col(0)); // cursor 1.97: not crossed yet
            engine.update(2.06, held_none()); // crossed, but no longer held
            engine.update(expire_after, held_none());
            TEST_CHECK(count_kind(engine, JudgmentKind::HitMine) == 0);
            TEST_CHECK(count_kind(engine, JudgmentKind::AvoidedMine) == 1);
        }

        // 16.7 / 16.10 Holding through a mine explodes it exactly once, at any
        // update rate.
        auto hold_through_mine = [&](double grid) {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 1.0, NoteType::HoldHead, 1.5));
            chart.notes.push_back(make_note(0, 2.0, NoteType::Mine));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(0, 1.0);
            const int steps = static_cast<int>(std::lround(1.2 / grid));
            for (int i = 0; i <= steps; ++i) {
                engine.update(1.0 + i * grid, held_col(0));
            }
            TEST_CHECK(count_kind(engine, JudgmentKind::HitMine) == 1);
            TEST_CHECK(count_kind(engine, JudgmentKind::HoldOk) == 1);
            TEST_CHECK(count_kind(engine, JudgmentKind::AvoidedMine) == 0);
            for (const blaze4k::JudgmentEvent& e : engine.events()) {
                if (e.kind == JudgmentKind::HitMine) {
                    TEST_CHECK(e.note_index == 1);
                    TEST_CHECK(e.hit_time_seconds >= 2.0 - 1e-9);
                    TEST_CHECK(e.hit_time_seconds <= 2.0 + grid + 1e-9);
                }
            }
        };
        hold_through_mine(0.01);
        hold_through_mine(0.1);
        hold_through_mine(0.001);

        // 16.8 A mine crossed unheld is never re-checked by a later hold.
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(1, 2.0, NoteType::Mine));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.update(2.1, held_none());
            engine.update(2.12, held_col(1)); // no press recorded: counts as held long enough
            engine.update(2.15, held_col(1));
            TEST_CHECK(count_kind(engine, JudgmentKind::HitMine) == 0);
        }

        // 16.9 Frame hitch: the crossing step measures the real distance.
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(2, 2.0, NoteType::Mine));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(2, 1.5);
            engine.update(1.9, held_col(2));
            engine.update(2.15, held_col(2)); // cursor 2.10: 0.10 s > mine window
            TEST_CHECK(count_kind(engine, JudgmentKind::HitMine) == 0);
            engine.update(expire_after, held_col(2));
            TEST_CHECK(count_kind(engine, JudgmentKind::HitMine) == 0);
            TEST_CHECK(count_kind(engine, JudgmentKind::AvoidedMine) == 1);

            blaze4k::JudgmentEngine fresh;
            fresh.reset(&chart, &k);
            fresh.handle_step(2, 1.5);
            fresh.update(1.9, held_col(2));
            fresh.update(2.11, held_col(2)); // cursor 2.06: inside the window
            TEST_CHECK(count_kind(fresh, JudgmentKind::HitMine) == 1);
            TEST_CHECK(approx(fresh.events().back().delta_ms, 60.0, 1e-6));
            TEST_CHECK(approx(fresh.events().back().hit_time_seconds, 2.11 - P));
        }

        // 16.11 OpenITG quirk (CrossedMineRow has no graded check): a held
        // crossing of an already-hit mine runs the full step, which can judge
        // the next same-column tap.
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 2.0, NoteType::Mine));
            chart.notes.push_back(make_note(0, 2.125, NoteType::Tap));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(0, 1.96);
            TEST_CHECK(engine.events().size() == 1);
            TEST_CHECK(engine.events().back().kind == JudgmentKind::HitMine);
            engine.update(1.97, held_col(0));
            engine.update(2.06, held_col(0)); // cursor 2.01: mine graded, tap 0.115 away
            TEST_CHECK(count_kind(engine, JudgmentKind::HitMine) == 1);
            TEST_CHECK(engine.events().back().kind == JudgmentKind::Tap);
            TEST_CHECK(engine.events().back().note_index == 1);
            TEST_CHECK(engine.events().back().window == blaze4k::TapJudgment::Decent);
        }

        // 16.12 A backward clock resync must not rewind the crossing cursor: a
        // mine crossed unheld is not crossed again after the clock jumps back.
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 2.0, NoteType::Mine));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.update(1.9, held_none());
            engine.update(2.06, held_none()); // cursor 2.01: crossed while unheld
            engine.update(1.95, held_none()); // backward resync
            engine.update(2.07, held_col(0)); // cursor 2.02: mine 0.02 s away
            engine.update(2.09, held_col(0));
            TEST_CHECK(count_kind(engine, JudgmentKind::HitMine) == 0);
            engine.update(expire_after, held_none());
            TEST_CHECK(count_kind(engine, JudgmentKind::AvoidedMine) == 1);
        }

        // 16.13 pad_stick == 0 (OpenITG IsButtonDown branch): the crossing runs at
        // the music time itself, and a press before it counts as held at once.
        {
            blaze4k::JudgmentConstants k0 = k;
            k0.windows.pad_stick = 0.0;
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, 1.95, NoteType::Tap));
            chart.notes.push_back(make_note(0, 2.0, NoteType::Mine));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k0);
            engine.update(1.9, held_none());
            engine.handle_step(0, 1.97); // the tap is closer than the mine
            TEST_CHECK(engine.events().size() == 1);
            TEST_CHECK(engine.events().back().kind == JudgmentKind::Tap);
            engine.update(2.01, held_col(0)); // with P = 0.05 the press would be too recent
            TEST_CHECK(count_kind(engine, JudgmentKind::HitMine) == 1);
            TEST_CHECK(engine.events().back().note_index == 1);
            TEST_CHECK(approx(engine.events().back().hit_time_seconds, 2.01));
            TEST_CHECK(approx(engine.events().back().delta_ms, 10.0, 1e-6));
        }

        std::cout << "  - Mine semantics match OpenITG Step/CrossedMineRow (pad-stick, once-only crossing).\n";
    }

    // 17. judge_window_scale/add reach every engine window read (OpenITG
    //     AdjustedWindowTap/Hold, Player.cpp:34-74; miss expiry via
    //     GetMaxStepDistanceSeconds, Player.cpp:1710-1713).
    {
        blaze4k::JudgmentConstants wide = blaze4k::JudgmentConstants::compiled_defaults();
        wide.windows.judge_window_add = 0.02;
        TEST_CHECK(wide.validate());
        const blaze4k::TimingWindows ww = wide.effective_windows();
        blaze4k::JudgmentConstants no_add = blaze4k::JudgmentConstants::compiled_defaults();
        no_add.windows.judge_window_add = 0.0;
        const double note_time = 2.0;

        // 17.1 Tap beyond the base Way Off, inside the widened one.
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(1, note_time, blaze4k::NoteType::Tap));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &wide);
            engine.handle_step(1, note_time + k.windows.way_off + 0.01);
            TEST_CHECK(engine.events().size() == 1);
            TEST_CHECK(engine.events().front().kind == blaze4k::JudgmentKind::Tap);
            TEST_CHECK(engine.events().front().window == blaze4k::TapJudgment::WayOff);
        }
        // 17.2 Mine beyond the base mine window, inside the widened one.
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(2, note_time, blaze4k::NoteType::Mine));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &wide);
            engine.handle_step(2, note_time + k.windows.hit_mine + 0.01);
            TEST_CHECK(engine.events().size() == 1);
            TEST_CHECK(engine.events().front().kind == blaze4k::JudgmentKind::HitMine);

            blaze4k::Chart base_chart;
            base_chart.notes.push_back(make_note(2, note_time, blaze4k::NoteType::Mine));
            blaze4k::JudgmentEngine base_engine;
            base_engine.reset(&base_chart, &no_add);
            base_engine.handle_step(2, note_time + k.windows.hit_mine + 0.01);
            TEST_CHECK(base_engine.events().empty());
        }
        // 17.3 Miss expiry waits for the widened Way Off.
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, note_time, blaze4k::NoteType::Tap));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &wide);
            engine.update(note_time + k.windows.way_off + 0.005, held_none());
            TEST_CHECK(engine.events().empty());
            engine.update(note_time + ww.way_off + 1e-6, held_none());
            TEST_CHECK(engine.events().size() == 1);
            const blaze4k::JudgmentEvent& e = engine.events().front();
            TEST_CHECK(e.kind == blaze4k::JudgmentKind::Miss);
            TEST_CHECK(approx(e.hit_time_seconds, note_time + ww.way_off));
            TEST_CHECK(approx(e.delta_ms, ww.way_off * 1000.0, 1e-6));
        }
        // 17.4 Hold OK window is widened too: released past the base window, not NG yet.
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, note_time, blaze4k::NoteType::HoldHead, 4.0));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &wide);
            engine.handle_step(0, note_time);
            engine.update(note_time + k.windows.hold_ok + 0.005, held_none());
            TEST_CHECK(engine.events().size() == 1);
            TEST_CHECK(!engine.is_note_judged(0));
            engine.update(note_time + ww.hold_ok + 0.01, held_none());
            TEST_CHECK(engine.events().size() == 2);
            TEST_CHECK(engine.events().back().kind == blaze4k::JudgmentKind::HoldNg);
        }
        // 17.5 Roll window is widened too.
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(0, note_time, blaze4k::NoteType::RollHead, 4.0));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &wide);
            engine.handle_step(0, note_time);
            engine.update(note_time + k.windows.hold_roll + 0.005, held_none());
            TEST_CHECK(engine.events().size() == 1);
            engine.update(note_time + ww.hold_roll + 0.01, held_none());
            TEST_CHECK(engine.events().size() == 2);
            TEST_CHECK(engine.events().back().kind == blaze4k::JudgmentKind::RollNg);
        }
        std::cout << "  - judge_window_scale/add reach tap, mine, expiry, hold and roll windows.\n";
    }

    // 18. Inclusive edges in the engine (OpenITG `<=`, Player.cpp:948,957-961;
    //     expiry only once strictly older than Way Off, Player.cpp:1373,1397).
    //     Notes sit at 0.0 so the engine's delta equals the probe time exactly
    //     in doubles, pinning the exact-edge comparisons.
    {
        const double note_time = 0.0;
        // 18.1 A step exactly at the Way Off edge is still Way Off.
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(1, note_time, blaze4k::NoteType::Tap));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(1, w.way_off);
            TEST_CHECK(engine.events().size() == 1);
            TEST_CHECK(engine.events().front().window == blaze4k::TapJudgment::WayOff);
        }
        // 18.2 A note exactly Way Off old has not expired yet.
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(1, note_time, blaze4k::NoteType::Tap));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.update(w.way_off, held_none());
            TEST_CHECK(engine.events().empty());
        }
        // 18.3 A mine stepped on exactly at the mine edge explodes.
        {
            blaze4k::Chart chart;
            chart.notes.push_back(make_note(1, note_time, blaze4k::NoteType::Mine));
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(1, w.hit_mine);
            TEST_CHECK(engine.events().size() == 1);
            TEST_CHECK(engine.events().front().kind == blaze4k::JudgmentKind::HitMine);
        }
        std::cout << "  - Engine window edges are inclusive.\n";
    }

    // 19. MercifulBeginner (#67; OpenITG Player.cpp:55-56,1089-1093,1710-1713).
    //     On a Beginner chart the effective Way Off gains +0.5 s (classification
    //     and miss expiry); an early Way Off is display-only and leaves the note
    //     live. Mine and hold windows are untouched.
    {
        const double note_time = 2.0;
        const double bw = k.effective_windows(true).way_off; // 0.6815
        TEST_CHECK(approx(bw, w.way_off + 0.5));
        auto make_chart = [](const char* difficulty, blaze4k::NoteType type, double end = 0.0) {
            blaze4k::Chart chart;
            chart.difficulty = difficulty;
            chart.notes.push_back(make_note(1, 2.0, type, end));
            return chart;
        };

        // 19.1 Late Way Off widened (Medium control: beyond Way Off -> no event).
        {
            blaze4k::Chart chart = make_chart("Beginner", blaze4k::NoteType::Tap);
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            TEST_CHECK(engine.is_beginner());
            engine.handle_step(1, 2.4);
            TEST_CHECK(engine.events().size() == 1);
            const blaze4k::JudgmentEvent& e = engine.events().front();
            TEST_CHECK(e.kind == blaze4k::JudgmentKind::Tap);
            TEST_CHECK(e.window == blaze4k::TapJudgment::WayOff);
            TEST_CHECK(approx(e.delta_ms, 400.0, 1e-6));
            TEST_CHECK(engine.is_note_judged(0));

            blaze4k::Chart medium = make_chart("Medium", blaze4k::NoteType::Tap);
            blaze4k::JudgmentEngine control;
            control.reset(&medium, &k);
            TEST_CHECK(!control.is_beginner());
            control.handle_step(1, 2.4);
            TEST_CHECK(control.events().empty());
        }
        // 19.2 Miss expiry waits for the widened Way Off.
        {
            blaze4k::Chart chart = make_chart("Beginner", blaze4k::NoteType::Tap);
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.update(note_time + w.way_off + 0.01, held_none());
            TEST_CHECK(engine.events().empty());
            engine.update(note_time + bw + 1e-6, held_none());
            TEST_CHECK(engine.events().size() == 1);
            const blaze4k::JudgmentEvent& e = engine.events().front();
            TEST_CHECK(e.kind == blaze4k::JudgmentKind::Miss);
            TEST_CHECK(approx(e.hit_time_seconds, note_time + bw));
            TEST_CHECK(approx(e.delta_ms, bw * 1000.0, 1e-6));

            blaze4k::Chart medium = make_chart("Medium", blaze4k::NoteType::Tap);
            blaze4k::JudgmentEngine control;
            control.reset(&medium, &k);
            control.update(note_time + w.way_off + 0.01, held_none());
            TEST_CHECK(control.events().size() == 1);
            TEST_CHECK(control.events().front().kind == blaze4k::JudgmentKind::Miss);
        }
        // 19.3 Early Way Off is display-only; the note stays live and steppable.
        {
            blaze4k::Chart chart = make_chart("Beginner", blaze4k::NoteType::Tap);
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(1, 1.6);
            TEST_CHECK(engine.events().empty());
            TEST_CHECK(engine.latest_event() == nullptr);
            TEST_CHECK(!engine.is_note_judged(0));
            TEST_CHECK(!engine.is_note_hidden(0));

            std::vector<blaze4k::JudgmentEvent> shown;
            engine.drain_display_only_events(shown);
            TEST_CHECK(shown.size() == 1);
            TEST_CHECK(shown.front().kind == blaze4k::JudgmentKind::Tap);
            TEST_CHECK(shown.front().window == blaze4k::TapJudgment::WayOff);
            TEST_CHECK(approx(shown.front().delta_ms, -400.0, 1e-6));
            TEST_CHECK(shown.front().note_index == 0);
            TEST_CHECK(shown.front().column == 1);
            TEST_CHECK(shown.front().note_type == blaze4k::NoteType::Tap);
            std::vector<blaze4k::JudgmentEvent> again;
            engine.drain_display_only_events(again);
            TEST_CHECK(again.empty());
            std::vector<blaze4k::JudgmentEvent> recorded;
            engine.drain_new_events(recorded);
            TEST_CHECK(recorded.empty());

            engine.handle_step(1, note_time);
            TEST_CHECK(engine.events().size() == 1);
            TEST_CHECK(engine.events().front().window == blaze4k::TapJudgment::Fantastic);
            engine.drain_display_only_events(again);
            TEST_CHECK(again.empty());
        }
        // 19.4 Early Way Off, then untouched: exactly one Miss at the widened expiry.
        {
            blaze4k::Chart chart = make_chart("Beginner", blaze4k::NoteType::Tap);
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(1, 1.7);
            TEST_CHECK(engine.events().empty());
            engine.update(note_time + bw - 0.01, held_none());
            TEST_CHECK(engine.events().empty());
            engine.update(note_time + bw + 1e-6, held_none());
            engine.update(note_time + bw + 0.5, held_none());
            TEST_CHECK(engine.events().size() == 1);
            TEST_CHECK(engine.events().front().kind == blaze4k::JudgmentKind::Miss);
        }
        // 19.5 Only an early Way Off is suppressed: early inside the base Way Off
        //      band too, but not an early Decent or any late Way Off.
        {
            blaze4k::Chart chart = make_chart("Beginner", blaze4k::NoteType::Tap);
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(1, 1.85);
            TEST_CHECK(engine.events().empty());
            std::vector<blaze4k::JudgmentEvent> shown;
            engine.drain_display_only_events(shown);
            TEST_CHECK(shown.size() == 1);

            blaze4k::Chart decent_chart = make_chart("Beginner", blaze4k::NoteType::Tap);
            blaze4k::JudgmentEngine decent;
            decent.reset(&decent_chart, &k);
            decent.handle_step(1, note_time - (w.great + w.decent) / 2.0);
            TEST_CHECK(decent.events().size() == 1);
            TEST_CHECK(decent.events().front().window == blaze4k::TapJudgment::Decent);

            blaze4k::Chart late_chart = make_chart("Beginner", blaze4k::NoteType::Tap);
            blaze4k::JudgmentEngine late;
            late.reset(&late_chart, &k);
            late.handle_step(1, 2.15);
            TEST_CHECK(late.events().size() == 1);
            TEST_CHECK(late.events().front().window == blaze4k::TapJudgment::WayOff);
            late.drain_display_only_events(shown);
            TEST_CHECK(shown.size() == 1);
        }
        // 19.6 Hold head early Way Off: no hold life starts; a later hit works normally.
        {
            blaze4k::Chart chart = make_chart("Beginner", blaze4k::NoteType::HoldHead, 4.0);
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(1, 1.7);
            TEST_CHECK(engine.events().empty());
            TEST_CHECK(!engine.is_hold_head_hit(0));
            TEST_CHECK(!engine.is_hold_in_progress(0));
            engine.handle_step(1, note_time);
            TEST_CHECK(engine.events().size() == 1);
            TEST_CHECK(engine.events().front().window == blaze4k::TapJudgment::Fantastic);
            engine.update(4.0, held_col(1));
            TEST_CHECK(engine.events().size() == 2);
            TEST_CHECK(engine.events().back().kind == blaze4k::JudgmentKind::HoldOk);
        }
        // 19.7 Mine window is not widened; avoided-mine expiry follows the widened Boo
        //      (Player.cpp:1397-1413).
        {
            blaze4k::Chart chart = make_chart("Beginner", blaze4k::NoteType::Mine);
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(1, note_time + w.hit_mine + 0.01);
            TEST_CHECK(engine.events().empty());
            engine.update(note_time + w.way_off + 0.01, held_none());
            TEST_CHECK(engine.events().empty());
            engine.update(note_time + bw + 1e-6, held_none());
            TEST_CHECK(engine.events().size() == 1);
            TEST_CHECK(engine.events().front().kind == blaze4k::JudgmentKind::AvoidedMine);
        }
        // 19.8 Flag off: a Beginner chart behaves exactly like the control.
        {
            blaze4k::JudgmentConstants off = blaze4k::JudgmentConstants::compiled_defaults();
            off.merciful_beginner = false;

            blaze4k::Chart chart = make_chart("Beginner", blaze4k::NoteType::Tap);
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &off);
            engine.handle_step(1, 1.85);
            TEST_CHECK(engine.events().size() == 1);
            TEST_CHECK(engine.events().front().window == blaze4k::TapJudgment::WayOff);
            std::vector<blaze4k::JudgmentEvent> shown;
            engine.drain_display_only_events(shown);
            TEST_CHECK(shown.empty());

            blaze4k::Chart late_chart = make_chart("Beginner", blaze4k::NoteType::Tap);
            blaze4k::JudgmentEngine late;
            late.reset(&late_chart, &off);
            late.handle_step(1, 2.4);
            TEST_CHECK(late.events().empty());
            late.update(note_time + w.way_off + 0.01, held_none());
            TEST_CHECK(late.events().size() == 1);
            TEST_CHECK(late.events().front().kind == blaze4k::JudgmentKind::Miss);
        }
        // 19.9 Detection mirrors StringToDifficulty + Steps::TidyUpData:
        //      label, then description, then meter 1 => Beginner.
        {
            struct Case {
                const char* label;
                const char* description;
                int meter;
                bool beginner;
            };
            const Case cases[] = {
                {"beginner", "", 5, true},
                {"BEGINNER", "", 5, true},
                {"Beginner", "", 5, true},
                {"Novice", "", 1, true},         // invalid label + meter 1
                {"Mystery", "Beginner", 4, true}, // invalid label -> description
                {"Novice", "", 2, false},        // invalid label + meter 2 -> Easy
                {"Easy", "Beginner", 1, false},  // valid label wins
                {"", "", 0, false},              // hand-built default -> Easy
            };
            for (const Case& c : cases) {
                blaze4k::Chart chart = make_chart(c.label, blaze4k::NoteType::Tap);
                chart.description = c.description;
                chart.meter = c.meter;
                blaze4k::JudgmentEngine engine;
                engine.reset(&chart, &k);
                TEST_CHECK(engine.is_beginner() == c.beginner);
                engine.handle_step(1, 2.4);
                TEST_CHECK(engine.events().size() == (c.beginner ? 1u : 0u));
            }
        }
        // 19.10 reset() clears the display-only queue.
        {
            blaze4k::Chart chart = make_chart("Beginner", blaze4k::NoteType::Tap);
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(1, 1.6);
            engine.reset(&chart, &k);
            std::vector<blaze4k::JudgmentEvent> shown;
            engine.drain_display_only_events(shown);
            TEST_CHECK(shown.empty());
            engine.reset(nullptr, &k);
            TEST_CHECK(!engine.is_beginner());
        }
        std::cout << "  - MercifulBeginner: widened Way Off, display-only early Way Off, controls.\n";
    }

    std::cout << "[judgment_engine_test] All judgment engine tests passed successfully!\n";
    return 0;
}
