#pragma once

#include <cstdint>

namespace blaze4k {

// Callback-anchored music-clock interpolation (#81, the "C" half of the #71
// decision). The audio thread captures an anchor at the end of every engine
// update: the monotonic time (read BEFORE the cursor) and the clock sound's
// cursor, plus the current device-callback interval estimate. The game thread
// then estimates `cursor + (now - anchor_ns) * rate`.
//
// AGENTS.md principle 1: the interpolated clock is re-anchored at every audio
// update, never decreases, and is bounded to one device period ahead of the
// latest anchor (the device-callback interval, not the engine update size).
// It is never a free-running wall clock.
//
// This module must remain free of wall-clock/frame-timing inputs: it includes
// only <cstdint>. Time arrives as uint64_t nanosecond parameters; the module
// never reads a clock itself.

struct ClockAnchor {
    uint64_t cursor_frames = 0;        // clock sound's cursor after this engine update (source-rate frames)
    uint64_t timestamp_ns = 0;         // injected monotonic ns, read on the audio thread BEFORE the cursor
    uint32_t period_engine_frames = 0; // device-callback interval estimate (engine-rate frames)
};

struct InterpolationInput {
    uint64_t raw_cursor_frames = 0;
    bool playing = false;
    bool anchor_valid = false;
    ClockAnchor anchor;
    uint64_t now_ns = 0;
    uint32_t source_rate = 0;   // the sound's own rate (cursor units)
    uint32_t engine_rate = 0;   // device/engine rate (period units)
    uint64_t length_frames = 0; // 0 = unknown
};

class ClockInterpolator {
public:
    // Seek/stop/play/resume/load/unload. Anchors captured before `now_ns` are
    // ignored afterwards; `floor_frames` is the new monotonic floor (may move
    // backwards, e.g. a backward seek).
    void reset(uint64_t now_ns, uint64_t floor_frames);

    // Rules (in order):
    //   1. !playing: floor = raw, return raw.
    //   2. est = raw.
    //   3. fresh anchor (ts >= reset_ns, rates > 0):
    //        est = max(raw, anchor.cursor + min(lead, cap)),
    //        lead = elapsed * source_rate / 1e9, cap = period * source_rate / engine_rate.
    //   4. length > 0: est = max(raw, min(est, length)).
    //   5. out = max(est, floor); floor = out.
    [[nodiscard]] uint64_t update(const InterpolationInput& in);

    [[nodiscard]] uint64_t floor_frames() const { return floor_; }
    [[nodiscard]] uint64_t reset_ns() const { return reset_ns_; }

private:
    uint64_t reset_ns_ = 0;
    uint64_t floor_ = 0;
};

} // namespace blaze4k
