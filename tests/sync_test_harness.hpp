#pragma once

// Permanent, deterministic sync regression harness (issue #15, PRD section 14).
//
// Drives a `MusicClock` from an injected fake `SamplePosition` source: the
// harness never opens an audio device, never creates a window, and never reads
// wall-clock or frame-delta time. Gameplay time is the music clock's time
// (frames / sample_rate + global_offset), so every result is a pure function of
// the frames the harness chooses to expose.
//
// Header-only and reusable by future timing tests. Includes no platform, audio,
// or wall-clock/frame-delta headers (AGENTS.md core principle 1).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "chart/chart.hpp"
#include "gameplay/judgment.hpp"
#include "gameplay/judgment_engine.hpp"
#include "timing/judgment_constants.hpp"
#include "timing/music_clock.hpp"

namespace blaze4k::sync_test {

// Fake audio clock: the injected SamplePosition source. Models the audio
// timebase as real_seconds * (1 + rate_error); the MusicClock then applies its
// own global_offset on top.
struct FakeSampleClock {
    uint64_t frames = 0;
    uint32_t sample_rate = 48000;
    double rate_error = 0.0;

    [[nodiscard]] uint64_t compute_frames(double real_seconds) const {
        return static_cast<uint64_t>(
            std::llround(real_seconds * static_cast<double>(sample_rate) * (1.0 + rate_error)));
    }

    // The music time the clock reads at `real_seconds` under `rate_error` and
    // `offset`, mirroring MusicClock::time_seconds exactly.
    [[nodiscard]] double music_time_at(double real_seconds, double rate_error,
                                       double offset) const {
        const double frames =
            std::llround(real_seconds * static_cast<double>(sample_rate) * (1.0 + rate_error));
        return frames / static_cast<double>(sample_rate) + offset;
    }
};

// One measured note: the chart's note time, the clock's hit time, and the
// derived ± delta (negative = early). `delta_ms` is measured from the clock for
// every note (so drift past the scoring window is still resolved); `scored` /
// `engine_delta_ms` / `window` capture the engine's own emitted Tap event when
// one exists.
struct NoteDelta {
    int column = 0;
    double note_seconds = 0.0;
    double hit_seconds = 0.0;
    double delta_ms = 0.0;        // measured from the clock
    double engine_delta_ms = 0.0; // engine-emitted delta; only meaningful if `scored`
    bool scored = false;          // the engine emitted a Tap event for this note
    TapJudgment window = TapJudgment::Miss;
};

struct SyncReport {
    std::vector<NoteDelta> notes;   // one entry per non-mine note, in note order
    double max_abs_delta_ms = 0.0;
    double terminal_delta_ms = 0.0; // last non-mine note
    int fantastic = 0;
    int non_fantastic = 0;
    int misses = 0;
};

// Expected drift for a note, in ms, derived independently from the fixture's
// beat math (note_beat * seconds_per_beat) plus the injected drift model inputs
// (rate_error, offset): (note_beat * seconds_per_beat * rate_error + offset) * 1000.
[[nodiscard]] inline double expected_drift_ms(double note_beat, double seconds_per_beat,
                                              double rate_error, double offset) {
    return (note_beat * seconds_per_beat * rate_error + offset) * 1000.0;
}

// Autoplays every non-mine note: at the note's *real* time the player hits the
// audible beat, so the input's music time is the clock reading there.
[[nodiscard]] inline SyncReport run_autoplay_sync(const Chart& chart,
                                                  const JudgmentConstants& constants,
                                                  double rate_error, double offset) {
    FakeSampleClock fake;
    fake.sample_rate = 48000;
    fake.rate_error = rate_error;
    fake.frames = 0;

    MusicClock clock([&fake] {
        return SamplePosition{fake.frames, fake.sample_rate};
    });
    clock.set_global_offset_seconds(offset);

    JudgmentEngine engine;
    engine.reset(&chart, &constants);

    SyncReport report;
    double last_hit = 0.0;

    for (std::size_t i = 0; i < chart.notes.size(); ++i) {
        const Note& note = chart.notes[i];
        if (note.type == NoteType::Mine) {
            continue; // defensive: fixture has none
        }

        fake.frames = fake.compute_frames(note.time_seconds);
        const double hit = clock.time_seconds();
        last_hit = hit;

        const std::size_t events_before = engine.events().size();
        engine.handle_step(note.column, hit);

        // `delta_ms` is measured from the clock for every note; the engine's own
        // emitted delta and verdict are captured when a Tap event is produced.
        NoteDelta delta;
        delta.column = note.column;
        delta.note_seconds = note.time_seconds;
        delta.hit_seconds = hit;
        delta.delta_ms = (hit - note.time_seconds) * 1000.0;
        delta.window = TapJudgment::Miss;

        for (std::size_t e = events_before; e < engine.events().size(); ++e) {
            const JudgmentEvent& event = engine.events()[e];
            if (event.kind == JudgmentKind::Tap &&
                event.note_index == static_cast<int>(i)) {
                delta.scored = true;
                delta.engine_delta_ms = event.delta_ms;
                delta.window = event.window;
                break;
            }
        }

        report.notes.push_back(delta);
        if (delta.window == TapJudgment::Fantastic) {
            ++report.fantastic;
        } else if (delta.window != TapJudgment::Miss) {
            ++report.non_fantastic;
        }
    }

    // Defensive sweep: any note still un-judged (e.g. drift past Way Off) must
    // surface as a Miss rather than silently vanish.
    const std::array<bool, 4> held_none{false, false, false, false};
    engine.update(last_hit, held_none);

    for (const JudgmentEvent& event : engine.events()) {
        if (event.kind == JudgmentKind::Miss) {
            ++report.misses;
        }
    }

    for (const NoteDelta& delta : report.notes) {
        report.max_abs_delta_ms = std::max(report.max_abs_delta_ms, std::fabs(delta.delta_ms));
    }
    if (!report.notes.empty()) {
        report.terminal_delta_ms = report.notes.back().delta_ms;
    }
    return report;
}

// True iff every note is a Tap exactly on an integer beat's time, one per beat,
// with column = beat % 4.
[[nodiscard]] inline bool is_pure_beats(const Chart& chart, int expected_beats, double eps) {
    if (static_cast<int>(chart.notes.size()) != expected_beats) {
        return false;
    }
    for (int i = 0; i < expected_beats; ++i) {
        const Note& note = chart.notes[static_cast<std::size_t>(i)];
        if (note.type != NoteType::Tap) {
            return false;
        }
        if (std::fabs(note.beat - static_cast<double>(i)) > eps) {
            return false;
        }
        if (std::fabs(note.time_seconds - static_cast<double>(i) * 0.5) > eps) {
            return false;
        }
        if (note.column != (i % 4)) {
            return false;
        }
    }
    return true;
}

} // namespace blaze4k::sync_test