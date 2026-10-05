// #81: pure tests for the callback-anchored music clock. CallbackGrouper (burst
// collapse), AnchorSlot (seqlock, incl. a 2-thread stress) and ClockInterpolator
// (every pinned rule on synthetic anchor sequences). No audio device, no clock.
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>

#include "audio/clock_anchor.hpp"
#include "timing/clock_interpolator.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr << "\n"; \
            std::abort(); \
        } \
    } while (0)

using blaze4k::AnchorSlot;
using blaze4k::CallbackGrouper;
using blaze4k::ClockAnchor;
using blaze4k::ClockInterpolator;
using blaze4k::InterpolationInput;

namespace {

constexpr uint64_t kMs = 1'000'000ULL;
constexpr uint64_t kUs = 1'000ULL;
constexpr uint32_t kRate = 48000;

uint64_t cap_frames(const ClockAnchor& a, uint32_t source_rate, uint32_t engine_rate) {
    return uint64_t{a.period_engine_frames} * source_rate / engine_rate;
}

// Drives a ClockInterpolator and checks the invariants every case shares:
// monotonic while playing, never below raw, never more than one period ahead
// of a fresh anchor.
struct Driver {
    ClockInterpolator interp;
    uint32_t source_rate = kRate;
    uint32_t engine_rate = kRate;
    uint64_t length = 0;
    bool have_anchor = false;
    ClockAnchor anchor;
    uint64_t raw = 0;
    uint64_t last_out = 0;
    bool have_last = false;

    void publish(uint64_t cursor, uint64_t ts, uint32_t period) {
        anchor = ClockAnchor{cursor, ts, period};
        have_anchor = true;
        raw = cursor; // the raw cursor moves at engine reads, i.e. with the anchors
    }

    uint64_t query(uint64_t now, bool playing = true) {
        InterpolationInput in;
        in.raw_cursor_frames = raw;
        in.playing = playing;
        in.anchor_valid = have_anchor;
        in.anchor = anchor;
        in.now_ns = now;
        in.source_rate = source_rate;
        in.engine_rate = engine_rate;
        in.length_frames = length;
        const uint64_t out = interp.update(in);
        if (playing) {
            TEST_CHECK(out >= raw);
            if (have_anchor && anchor.timestamp_ns >= interp.reset_ns() && source_rate > 0 &&
                engine_rate > 0) {
                TEST_CHECK(out <= std::max(raw, anchor.cursor_frames +
                                                    cap_frames(anchor, source_rate, engine_rate)) ||
                           out == last_out); // only the floor may hold a value above the bound
            }
            if (have_last) {
                TEST_CHECK(out >= last_out);
            }
            last_out = out;
            have_last = true;
        } else {
            TEST_CHECK(out == raw);
            have_last = false;
        }
        return out;
    }

    void reset(uint64_t now, uint64_t floor) {
        interp.reset(now, floor);
        raw = floor;
        have_last = false;
    }
};

void test_grouper() {
    // Steady 480-frame updates every 10 ms.
    {
        CallbackGrouper g;
        TEST_CHECK(g.on_update(480, 0, kRate) == 480);
        for (int k = 1; k <= 100; ++k) {
            TEST_CHECK(g.on_update(480, static_cast<uint64_t>(k) * 10 * kMs, kRate) == 480);
        }
        const CallbackGrouper::Stats s = g.stats();
        TEST_CHECK(s.min_group == 480);
        TEST_CHECK(s.max_group == 480);
        TEST_CHECK(s.callbacks == 100); // completed groups (the 101st is still open)
    }
    // Burst: two 128-frame engine updates 20 us apart in each 5.33 ms device callback.
    {
        CallbackGrouper g;
        const uint64_t interval = 256ULL * 1'000'000'000ULL / kRate;
        for (int k = 0; k < 50; ++k) {
            const uint64_t t = static_cast<uint64_t>(k) * interval;
            const uint32_t first = g.on_update(128, t, kRate);
            const uint32_t second = g.on_update(128, t + 20 * kUs, kRate);
            TEST_CHECK(second == 256);
            if (k == 0) {
                TEST_CHECK(first == 128);
            } else {
                TEST_CHECK(first == 256); // prev_group covers the window after the first update
            }
        }
        const CallbackGrouper::Stats s = g.stats();
        TEST_CHECK(s.min_group == 256 && s.max_group == 256 && s.callbacks == 49);
    }
    // 0-frame updates (nothing playing) and a 0 engine rate are ignored and do
    // not move last_update_ns.
    {
        CallbackGrouper g;
        TEST_CHECK(g.on_update(480, 0, kRate) == 480);
        TEST_CHECK(g.on_update(0, 9 * kMs, kRate) == 480);
        TEST_CHECK(g.on_update(480, 9 * kMs, 0) == 480);
        // 10 ms after the last real update: a new group, not a burst at 9 ms + 1 ms.
        TEST_CHECK(g.on_update(480, 10 * kMs, kRate) == 480);
        TEST_CHECK(g.stats().callbacks == 1);
    }
    // Early callback (4 ms instead of 10) merges into the previous group: the
    // estimate loosens to at most 2 periods for one interval, then recovers.
    {
        CallbackGrouper g;
        g.on_update(480, 0, kRate);
        g.on_update(480, 10 * kMs, kRate);
        g.on_update(480, 20 * kMs, kRate);
        const uint32_t early = g.on_update(480, 24 * kMs, kRate);
        TEST_CHECK(early <= 960 && early >= 480);
        TEST_CHECK(g.on_update(480, 34 * kMs, kRate) <= 960);
        TEST_CHECK(g.on_update(480, 44 * kMs, kRate) == 480);
        const CallbackGrouper::Stats s = g.stats();
        TEST_CHECK(s.min_group == 480 && s.max_group == 960 && s.callbacks == 4);
        g.reset();
        TEST_CHECK(g.stats().callbacks == 0 && g.stats().min_group == 0);
    }
    std::cout << "  - 1. CallbackGrouper: steady, burst collapse, 0-frame ignore, early merge, stats ok.\n";
}

void test_seqlock() {
    {
        AnchorSlot slot;
        ClockAnchor out{1, 2, 3};
        TEST_CHECK(!slot.try_read(out));
        TEST_CHECK(out.cursor_frames == 1); // untouched on failure
        slot.publish(ClockAnchor{480, 123456789, 256});
        TEST_CHECK(slot.try_read(out));
        TEST_CHECK(out.cursor_frames == 480 && out.timestamp_ns == 123456789 &&
                   out.period_engine_frames == 256);
    }
    {
        AnchorSlot slot;
        constexpr uint64_t kWrites = 1'000'000;
        std::atomic<bool> done{false};
        std::thread writer([&] {
            for (uint64_t i = 1; i <= kWrites; ++i) {
                slot.publish(ClockAnchor{i, i * 7, static_cast<uint32_t>(i % 1000)});
            }
            done.store(true, std::memory_order_release);
        });
        uint64_t successes = 0;
        uint64_t last_cursor = 0;
        while (!done.load(std::memory_order_acquire)) {
            ClockAnchor a;
            if (slot.try_read(a)) {
                TEST_CHECK(a.timestamp_ns == a.cursor_frames * 7);
                TEST_CHECK(a.period_engine_frames == a.cursor_frames % 1000);
                TEST_CHECK(a.cursor_frames >= last_cursor); // single writer: never older
                last_cursor = a.cursor_frames;
                ++successes;
            }
        }
        writer.join();
        ClockAnchor final_anchor;
        TEST_CHECK(slot.try_read(final_anchor));
        TEST_CHECK(final_anchor.cursor_frames == kWrites);
        ++successes;
        TEST_CHECK(successes > 0);
        std::cout << "  - 2. AnchorSlot: round trip, empty read false, 2-thread stress ok ("
                  << successes << " consistent reads).\n";
    }
}

void test_steady() {
    Driver d;
    uint64_t prev = 0;
    for (uint64_t t_ms = 0; t_ms <= 1000; ++t_ms) {
        if (t_ms % 10 == 0) {
            d.publish(t_ms * 48, t_ms * kMs, 480);
        }
        const uint64_t out = d.query(t_ms * kMs);
        const uint64_t ideal = t_ms * 48;
        TEST_CHECK(out <= ideal + 1 && out + 1 >= ideal);
        if (t_ms > 0) {
            TEST_CHECK(out > prev); // never stalls
        }
        prev = out;
    }
    // Sub-ms query on the line.
    d.publish(1010 * 48, 1010 * kMs, 480);
    TEST_CHECK(d.query(1010 * kMs + 500 * kUs) == 1010 * 48 + 24);
    std::cout << "  - 3. Steady anchors: error <= 1 frame vs the ideal line, strictly increasing.\n";
}

void test_late_callback() {
    Driver d;
    uint64_t prev = 0;
    for (uint64_t t_ms = 0; t_ms <= 600; ++t_ms) {
        if (t_ms % 10 == 0 && t_ms != 500) {
            d.publish(t_ms * 48, t_ms * kMs, 480);
        }
        if (t_ms == 508) {
            d.publish(500 * 48, 508 * kMs, 480); // 8 ms late, on-schedule cursor
        }
        const uint64_t out = d.query(t_ms * kMs);
        if (t_ms > 0) {
            TEST_CHECK(out >= prev);
            TEST_CHECK(out - prev <= 480); // no single-query jump beyond one period
        }
        if (t_ms >= 510) {
            const uint64_t ideal = t_ms * 48;
            TEST_CHECK(out <= ideal + 1 && out + 1 >= ideal); // back on the line
        }
        prev = out;
    }
    std::cout << "  - 4. Late callback: no decrease, no jump > cap, back on the line next anchor.\n";
}

// Returns the number of 0.5 ms query steps (after warm-up) where the output
// did not advance.
int run_burst(uint32_t forced_period) {
    Driver d;
    CallbackGrouper g;
    const uint64_t interval = 256ULL * 1'000'000'000ULL / kRate; // 5.333 ms
    uint64_t cursor = 0;
    uint64_t next_cb = 0;
    uint64_t prev = 0;
    int stalls = 0;
    for (uint64_t t = 0; t <= 300 * kMs; t += 500 * kUs) {
        while (next_cb <= t) {
            for (int burst = 0; burst < 2; ++burst) {
                const uint64_t ts = next_cb + static_cast<uint64_t>(burst) * 20 * kUs;
                cursor += 128;
                const uint32_t period = g.on_update(128, ts, kRate);
                d.publish(cursor, ts, forced_period != 0 ? forced_period : period);
            }
            next_cb += interval;
        }
        const uint64_t out = d.query(t);
        if (t > 20 * kMs && out == prev) {
            ++stalls;
        }
        prev = out;
    }
    return stalls;
}

void test_burst() {
    const int grouped_stalls = run_burst(0);
    const int per_update_stalls = run_burst(128);
    TEST_CHECK(grouped_stalls == 0);
    TEST_CHECK(per_update_stalls > 100); // the per-update cap stalls ~half of every interval
    std::cout << "  - 5. Burst: grouped period (256) never stalls; per-update cap 128 stalls "
              << per_update_stalls << " steps.\n";
}

void test_stall() {
    Driver d;
    for (uint64_t t_ms = 0; t_ms <= 500; t_ms += 10) {
        d.publish(t_ms * 48, t_ms * kMs, 480);
        (void)d.query(t_ms * kMs);
    }
    const uint64_t frozen = 500 * 48 + 480;
    for (uint64_t t_ms = 501; t_ms < 600; ++t_ms) {
        TEST_CHECK(d.query(t_ms * kMs) == std::min<uint64_t>(frozen, 500 * 48 + (t_ms - 500) * 48));
    }
    TEST_CHECK(d.query(599 * kMs) == frozen);
    // Anchors resume; the underrun lost device time, so the cursor continues
    // from where the device stopped (below the frozen value at first).
    uint64_t cursor = 500 * 48;
    for (uint64_t t_ms = 600; t_ms <= 700; ++t_ms) {
        if (t_ms % 10 == 0) {
            cursor += 480;
            d.publish(cursor, t_ms * kMs, 480);
        }
        TEST_CHECK(d.query(t_ms * kMs) >= frozen);
    }
    TEST_CHECK(d.last_out > frozen);
    std::cout << "  - 6. Stall/underrun: frozen at anchor + cap, monotonic after resume.\n";
}

void test_pause_seek() {
    // Paused / not playing: raw, floor follows raw.
    {
        Driver d;
        d.publish(48000, 1000 * kMs, 480);
        TEST_CHECK(d.query(1005 * kMs, /*playing=*/false) == 48000);
        TEST_CHECK(d.interp.floor_frames() == 48000);
    }
    // Forward seek: a pre-reset anchor is ignored, the output is the raw (seek target).
    {
        Driver d;
        d.publish(48000, 995 * kMs, 480);
        (void)d.query(996 * kMs);
        d.reset(1000 * kMs, 96000);
        TEST_CHECK(d.query(1002 * kMs) == 96000);
        d.publish(96000 + 480, 1010 * kMs, 480);
        TEST_CHECK(d.query(1012 * kMs) == 96000 + 480 + 96);
    }
    // Backward seek works through reset (the floor moves back).
    {
        Driver d;
        for (uint64_t t_ms = 0; t_ms <= 1000; t_ms += 10) {
            d.publish(t_ms * 48, t_ms * kMs, 480);
            (void)d.query(t_ms * kMs + 5 * kMs);
        }
        TEST_CHECK(d.last_out > 40000);
        d.reset(1006 * kMs, 0);
        TEST_CHECK(d.query(1007 * kMs) == 0); // stale anchor ignored
        d.publish(480, 1016 * kMs, 480);
        TEST_CHECK(d.query(1017 * kMs) == 480 + 48);
    }
    // Resume after pause: the stale pre-pause anchor must not add a period.
    {
        Driver d;
        d.publish(48000, 1000 * kMs, 480);
        (void)d.query(1001 * kMs);
        TEST_CHECK(d.query(1500 * kMs, /*playing=*/false) == 48000);
        d.reset(3000 * kMs, 48000); // resume
        TEST_CHECK(d.query(3001 * kMs) == 48000); // not 48000 + 480
        d.publish(48480, 3010 * kMs, 480);
        TEST_CHECK(d.query(3011 * kMs) == 48480 + 48);
    }
    std::cout << "  - 7. Pause/seek: raw while paused, pre-reset anchors ignored, backward seek, no resume jump.\n";
}

void test_song_end() {
    Driver d;
    d.length = 96000;
    d.publish(95900, 2000 * kMs, 480);
    TEST_CHECK(d.query(2009 * kMs) == 96000); // 95900 + 432 clamped to the length
    d.raw = 96000;
    TEST_CHECK(d.query(2020 * kMs, /*playing=*/false) == 96000);
    // A raw cursor past an (inexact) length still wins over the clamp.
    Driver e;
    e.length = 1000;
    e.publish(1200, 0, 480);
    TEST_CHECK(e.query(5 * kMs) == 1200);
    std::cout << "  - 8. Song end: clamped to length_frames; raw once stopped.\n";
}

void test_resampling() {
    Driver d;
    d.source_rate = 44100;
    d.engine_rate = 48000;
    d.publish(44100, 1000 * kMs, 480);
    TEST_CHECK(cap_frames(d.anchor, 44100, 48000) == 441);
    TEST_CHECK(d.query(1005 * kMs) == 44100 + 220); // 5 ms at 44.1 kHz = 220.5
    TEST_CHECK(d.query(1020 * kMs) == 44100 + 441); // capped at 441 source frames
    std::cout << "  - 9. Resampling: cap 441 source frames, lead at 44.1 kHz.\n";
}

void test_edges() {
    // 10. Race window: the cursor advanced inside an engine read before onProcess published.
    {
        Driver d;
        d.publish(500, 1000 * kMs, 480);
        d.raw = 1000;
        TEST_CHECK(d.query(1000 * kMs) == 1000);
    }
    // 11. Clock skew: now before the anchor timestamp gives elapsed 0.
    {
        Driver d;
        d.publish(4800, 1000 * kMs, 480);
        TEST_CHECK(d.query(999 * kMs) == 4800);
    }
    // 12. Zero rates: raw.
    {
        Driver d;
        d.source_rate = 0;
        d.publish(4800, 1000 * kMs, 480);
        d.raw = 4700;
        TEST_CHECK(d.query(1005 * kMs) == 4700);
        Driver e;
        e.engine_rate = 0;
        e.publish(4800, 1000 * kMs, 480);
        e.raw = 4700;
        TEST_CHECK(e.query(1005 * kMs) == 4700);
    }
    // Overflow safety: a very long stall still caps (no wrap).
    {
        Driver d;
        d.publish(4800, 1, 480);
        TEST_CHECK(d.query(UINT64_MAX) == 4800 + 480);
    }
    std::cout << "  - 10-12. Race window raw wins, clock skew elapsed 0, zero rates raw, overflow-safe.\n";
}

} // namespace

int main() {
    std::cout << "[clock_interpolation_test] Starting...\n";
    test_grouper();
    test_seqlock();
    test_steady();
    test_late_callback();
    test_burst();
    test_stall();
    test_pause_seek();
    test_song_end();
    test_resampling();
    test_edges();
    std::cout << "[clock_interpolation_test] All tests passed.\n";
    return 0;
}
