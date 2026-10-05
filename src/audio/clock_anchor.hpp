#pragma once

#include <atomic>
#include <cstdint>

#include "timing/clock_interpolator.hpp"

struct ma_sound;

namespace blaze4k {

// Audio-thread state for the callback-anchored music clock (#81). Pure: no
// miniaudio, no SDL, no locks, no allocation, no logging.

// Merges engine updates that arrive back to back within one device callback
// into a single group, so the interpolation cap is the device-callback interval
// (AGENTS.md principle 1), not the engine update size. Written on the audio
// thread only; the stats are atomics readable from the game thread.
class CallbackGrouper {
public:
    struct Stats {
        uint32_t min_group = 0; // 0 = no completed group yet
        uint32_t max_group = 0;
        uint64_t callbacks = 0; // completed groups
    };

    // Returns the current device-period estimate in engine frames:
    // max(current group, previous group). `frames == 0` (nothing playing) and
    // `engine_rate == 0` are ignored.
    uint32_t on_update(uint64_t frames, uint64_t now_ns, uint32_t engine_rate) noexcept;

    [[nodiscard]] Stats stats() const noexcept;
    void reset() noexcept;
    // Audio thread: zeroes only the stats; the period estimate is kept.
    void reset_stats() noexcept;

private:
    bool has_group_ = false;
    uint64_t last_update_ns_ = 0;
    uint32_t group_ = 0;
    uint32_t prev_group_ = 0;
    std::atomic<uint32_t> min_group_{0};
    std::atomic<uint32_t> max_group_{0};
    std::atomic<uint64_t> callbacks_{0};
};

// Single-writer seqlock carrying the latest ClockAnchor from the audio thread
// to the game thread. Every field is atomic, so there is no data race in the
// C++ memory-model sense.
class AnchorSlot {
public:
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    static_assert(std::atomic<uint32_t>::is_always_lock_free);

    static constexpr int kMaxReadAttempts = 8;

    // Audio thread, wait-free.
    void publish(const ClockAnchor& anchor) noexcept;

    // Game thread. False if nothing was ever published or every attempt raced a
    // write; the caller then falls back to the raw cursor.
    bool try_read(ClockAnchor& out) const noexcept;

private:
    std::atomic<uint64_t> seq_{0};
    std::atomic<uint64_t> cursor_frames_{0};
    std::atomic<uint64_t> timestamp_ns_{0};
    std::atomic<uint32_t> period_engine_frames_{0};
};

// One registered clock source: the sound whose cursor the audio thread samples
// and the slot it publishes into. Owned by a SoundStream via unique_ptr so the
// address is stable across moves. `sound` is atomic because the game thread may
// (re)write it while the tap is active.
struct ClockTap {
    static_assert(std::atomic<ma_sound*>::is_always_lock_free);

    std::atomic<ma_sound*> sound{nullptr};
    AnchorSlot slot;
};

} // namespace blaze4k
