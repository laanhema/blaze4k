#include <cmath>
#include <filesystem>
#include <iostream>
#include <vector>

#include "chart/simfile_parser.hpp"
#include "gameplay/judgment.hpp"
#include "timing/judgment_constants.hpp"
#include "sync_test_harness.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

constexpr int kExpectedBeats = 240;
constexpr int kSampleRate = 48000;

bool approx(double a, double b, double epsilon) {
    return std::fabs(a - b) < epsilon;
}

} // namespace

int main() {
    std::cout << "[metronome_sync_test] Starting metronome sync regression tests...\n";
    const blaze4k::JudgmentConstants& k = blaze4k::JudgmentConstants::compiled_defaults();
    // Sync tolerance uses the base Fantastic window (21.5 ms), which is stricter
    // than the effective one (base * scale + add = 23.0 ms with the cabinet add).
    const double one_window_ms = k.windows.fantastic * 1000.0;
    // The classification boundary is the effective window (OpenITG
    // ADJUSTED_WINDOW_TAP(TW_Marvelous), Player.cpp:34-58, 957).
    const double fw = k.effective_windows().fantastic;

    // Resolve the fixture from whatever directory CTest launched the binary in.
    std::filesystem::path ref = "tests/fixtures/sync_test/metronome.sm";
    if (!std::filesystem::exists(ref)) {
        ref = "../tests/fixtures/sync_test/metronome.sm";
    }
    if (!std::filesystem::exists(ref)) {
        ref = "../../tests/fixtures/sync_test/metronome.sm";
    }
    TEST_CHECK(std::filesystem::exists(ref));

    blaze4k::SimfileParser parser;
    TEST_CHECK(parser.parse_file(ref.string()));
    TEST_CHECK(parser.charts().size() == 1);
    const blaze4k::Chart& chart = parser.charts()[0];

    // 1. Fixture integrity / AC1: full-length on-beat metronome.
    {
        TEST_CHECK(chart.steps_type == "dance-single");
        TEST_CHECK(chart.difficulty == "Challenge");
        TEST_CHECK(chart.notes.size() == static_cast<std::size_t>(kExpectedBeats));
        TEST_CHECK(chart.tap_count == kExpectedBeats);
        TEST_CHECK(chart.mine_count == 0);
        TEST_CHECK(chart.hold_count == 0);
        TEST_CHECK(chart.roll_count == 0);
        TEST_CHECK(blaze4k::sync_test::is_pure_beats(chart, kExpectedBeats, 1e-9));

        TEST_CHECK(chart.timing.stops().empty());
        TEST_CHECK(!chart.timing.has_exotic_timing());
        TEST_CHECK(chart.timing.bpms().size() == 1);
        TEST_CHECK(approx(chart.timing.bpms().front().bpm, 120.0, 1e-12));
        TEST_CHECK(approx(chart.timing.offset(), 0.0, 1e-12));

        for (int i = 0; i < kExpectedBeats; ++i) {
            const blaze4k::Note& note = chart.notes[static_cast<std::size_t>(i)];
            TEST_CHECK(approx(note.time_seconds, note.beat * 0.5, 1e-9));
            TEST_CHECK(note.column == (i % 4));
        }

        // Same-column gap (2.0 s) is twice the engine's ±1.0 s search radius; the
        // drift exercised by this test stays far inside that bound (< Way Off,
        // 0.18 s), so a hit can never select a neighbouring note.
        for (int i = 0; i + 4 < kExpectedBeats; ++i) {
            const double gap = chart.notes[static_cast<std::size_t>(i + 4)].time_seconds -
                               chart.notes[static_cast<std::size_t>(i)].time_seconds;
            TEST_CHECK(gap >= 2.0 - 1e-9);
        }
        std::cout << "  - AC1: 240 on-beat taps over 120 s, single BPM, no stops/mines/holds.\n";
    }

    // 2. Zero-drift full-song play stays within one judgment window (PRD §11).
    {
        const blaze4k::sync_test::SyncReport report =
            blaze4k::sync_test::run_autoplay_sync(chart, k, 0.0, 0.0);
        TEST_CHECK(report.notes.size() == static_cast<std::size_t>(kExpectedBeats));
        TEST_CHECK(report.fantastic == kExpectedBeats);
        TEST_CHECK(report.non_fantastic == 0);
        TEST_CHECK(report.misses == 0);
        for (const blaze4k::sync_test::NoteDelta& d : report.notes) {
            TEST_CHECK(d.scored);
            TEST_CHECK(approx(d.engine_delta_ms, 0.0, 0.05));
            TEST_CHECK(d.window == blaze4k::TapJudgment::Fantastic);
        }
        // Frame quantization at 48 kHz is <= 1 frame (~0.0208 ms); require
        // well under that, and far under one 21.5 ms window.
        TEST_CHECK(report.max_abs_delta_ms < 0.05);
        TEST_CHECK(report.max_abs_delta_ms < one_window_ms);
        std::cout << "  - Zero-drift sync holds: 240 Fantastic, max |delta| = "
                  << report.max_abs_delta_ms << " ms (< " << one_window_ms << " ms).\n";
    }

    // 3. Constant offset drift / AC2: revealed at sub-window resolution.
    {
        for (double d : {0.010, -0.010}) {
            const blaze4k::sync_test::SyncReport report =
                blaze4k::sync_test::run_autoplay_sync(chart, k, 0.0, d);
            TEST_CHECK(report.notes.size() == static_cast<std::size_t>(kExpectedBeats));
            TEST_CHECK(report.misses == 0);
            for (const blaze4k::sync_test::NoteDelta& delta : report.notes) {
                TEST_CHECK(approx(delta.delta_ms, d * 1000.0, 0.05));
                TEST_CHECK(delta.scored);
                TEST_CHECK(approx(delta.engine_delta_ms, d * 1000.0, 0.05));
                TEST_CHECK(delta.window == blaze4k::TapJudgment::Fantastic);
            }
            TEST_CHECK(approx(report.max_abs_delta_ms, 10.0, 0.05));
            TEST_CHECK(report.max_abs_delta_ms < one_window_ms);
        }

        // Window-boundary sweep: one-window resolution flips the classification
        // exactly where B2's `classify_tap` boundary lies.
        struct SweepCase {
            double offset;
            blaze4k::TapJudgment expected;
        };
        const SweepCase sweep[] = {
            {0.500 * fw, blaze4k::TapJudgment::Fantastic},
            {0.999 * fw, blaze4k::TapJudgment::Fantastic},
            {1.001 * fw, blaze4k::TapJudgment::Excellent},
        };
        for (const SweepCase& c : sweep) {
            const blaze4k::sync_test::SyncReport report =
                blaze4k::sync_test::run_autoplay_sync(chart, k, 0.0, c.offset);
            TEST_CHECK(report.notes.size() == static_cast<std::size_t>(kExpectedBeats));
            for (const blaze4k::sync_test::NoteDelta& delta : report.notes) {
                TEST_CHECK(delta.window == c.expected);
            }
        }
        std::cout << "  - AC2: constant offset d=±10 ms measured on every note; "
                     "window flips at 1.0x fantastic boundary.\n";
    }

    // 4. Progressive rate drift / AC2: accumulates linearly, flags > one window.
    {
        const double rate_error = 0.005;
        const double seconds_per_beat = 60.0 / chart.timing.bpms().front().bpm;
        const blaze4k::sync_test::SyncReport report =
            blaze4k::sync_test::run_autoplay_sync(chart, k, rate_error, 0.0);
        TEST_CHECK(report.notes.size() == static_cast<std::size_t>(kExpectedBeats));

        double previous = -1e18;
        int flagged = 0;
        for (std::size_t i = 0; i < report.notes.size(); ++i) {
            // Reference drift from the fixture's beat math, not from the harness.
            const double expected = blaze4k::sync_test::expected_drift_ms(
                static_cast<double>(i), seconds_per_beat, rate_error, 0.0);
            // Scored notes must match the engine's own emitted delta; unscored
            // notes (drift past Way Off) only have the measured clock delta.
            const double observed = report.notes[i].scored
                                        ? report.notes[i].engine_delta_ms
                                        : report.notes[i].delta_ms;
            TEST_CHECK(approx(observed, expected, 0.05));
            TEST_CHECK(report.notes[i].delta_ms >= previous - 1e-9);
            previous = report.notes[i].delta_ms;
            if (report.notes[i].window == blaze4k::TapJudgment::Miss) {
                ++flagged;
            }
        }
        // First note is at t=0: no accumulated drift.
        TEST_CHECK(std::fabs(report.notes.front().delta_ms) < 1e-6);
        TEST_CHECK(report.notes.front().scored);

        const double analytic_terminal =
            blaze4k::sync_test::expected_drift_ms(239.0, seconds_per_beat, rate_error, 0.0); // 597.5 ms
        TEST_CHECK(approx(report.terminal_delta_ms, analytic_terminal, 0.05));
        TEST_CHECK(report.terminal_delta_ms > one_window_ms);
        // The engine itself flags the regression: once accumulated drift passes
        // Way Off, the late notes are no longer scored as taps.
        TEST_CHECK(flagged > 0);
        TEST_CHECK(report.misses == flagged);
        std::cout << "  - AC2: 0.5% rate drift terminal delta = " << report.terminal_delta_ms
                  << " ms (analytic " << analytic_terminal << " ms) > " << one_window_ms
                  << " ms window; " << flagged << " late notes flagged (unscored).\n";
    }

    // 5. Within-tolerance rate drift passes: 1 ppm does not break sync.
    {
        const blaze4k::sync_test::SyncReport report =
            blaze4k::sync_test::run_autoplay_sync(chart, k, 1e-6, 0.0);
        TEST_CHECK(report.notes.size() == static_cast<std::size_t>(kExpectedBeats));
        for (const blaze4k::sync_test::NoteDelta& d : report.notes) {
            TEST_CHECK(d.scored);
            TEST_CHECK(approx(d.engine_delta_ms, d.delta_ms, 0.05));
        }
        TEST_CHECK(report.max_abs_delta_ms < one_window_ms);
        TEST_CHECK(approx(report.terminal_delta_ms, 0.1195, 0.05));
        std::cout << "  - 1 ppm rate error: terminal delta = " << report.terminal_delta_ms
                  << " ms (< " << one_window_ms << " ms window); sync holds.\n";
    }

    // 6. Determinism: identical runs, bit-for-bit, with no wall-clock input.
    {
        const blaze4k::sync_test::SyncReport a =
            blaze4k::sync_test::run_autoplay_sync(chart, k, 0.005, 0.0);
        const blaze4k::sync_test::SyncReport b =
            blaze4k::sync_test::run_autoplay_sync(chart, k, 0.005, 0.0);
        TEST_CHECK(a.notes.size() == b.notes.size());
        TEST_CHECK(a.fantastic == b.fantastic);
        TEST_CHECK(a.non_fantastic == b.non_fantastic);
        TEST_CHECK(a.misses == b.misses);
        for (std::size_t i = 0; i < a.notes.size(); ++i) {
            TEST_CHECK(a.notes[i].delta_ms == b.notes[i].delta_ms);
            TEST_CHECK(a.notes[i].engine_delta_ms == b.notes[i].engine_delta_ms);
            TEST_CHECK(a.notes[i].hit_seconds == b.notes[i].hit_seconds);
            TEST_CHECK(a.notes[i].window == b.notes[i].window);
        }
        std::cout << "  - Determinism: repeated runs byte-identical (no wall-clock/frame input).\n";
    }

    // 7. No audio device dependency: enforced by construction (harness binds a
    //    fake SamplePosition source and includes no platform or wall-clock headers).
    {
        const blaze4k::sync_test::FakeSampleClock fake;
        TEST_CHECK(approx(fake.music_time_at(119.5, 0.0, 0.0), 119.5, 1e-9));
        TEST_CHECK(approx(fake.music_time_at(0.0, 0.0, 0.01), 0.01, 1e-9));
        std::cout << "  - No audio device: clock source is an injected fake provider.\n";
    }

    std::cout << "[metronome_sync_test] All metronome sync regression tests passed!\n";
    return 0;
}