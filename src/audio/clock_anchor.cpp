#include "audio/clock_anchor.hpp"

#include <algorithm>

namespace blaze4k {

uint32_t CallbackGrouper::on_update(uint64_t frames, uint64_t now_ns,
                                    uint32_t engine_rate) noexcept {
    if (frames == 0 || engine_rate == 0) {
        return std::max(group_, prev_group_);
    }
    const auto frames32 = static_cast<uint32_t>(std::min<uint64_t>(frames, UINT32_MAX));
    // Half of this update's duration: a following update closer than this is a
    // burst within the same device callback.
    const uint64_t gap_ns = frames * 1'000'000'000ULL / engine_rate / 2;

    if (!has_group_ || now_ns - last_update_ns_ > gap_ns || now_ns < last_update_ns_) {
        if (has_group_) {
            prev_group_ = group_;
            const uint32_t lo = min_group_.load(std::memory_order_relaxed);
            if (lo == 0 || group_ < lo) {
                min_group_.store(group_, std::memory_order_relaxed);
            }
            if (group_ > max_group_.load(std::memory_order_relaxed)) {
                max_group_.store(group_, std::memory_order_relaxed);
            }
            callbacks_.store(callbacks_.load(std::memory_order_relaxed) + 1,
                             std::memory_order_relaxed);
        }
        group_ = frames32;
        has_group_ = true;
    } else {
        group_ = static_cast<uint32_t>(std::min<uint64_t>(uint64_t{group_} + frames32, UINT32_MAX));
    }
    last_update_ns_ = now_ns;
    return std::max(group_, prev_group_);
}

CallbackGrouper::Stats CallbackGrouper::stats() const noexcept {
    return Stats{min_group_.load(std::memory_order_relaxed),
                 max_group_.load(std::memory_order_relaxed),
                 callbacks_.load(std::memory_order_relaxed)};
}

void CallbackGrouper::reset() noexcept {
    has_group_ = false;
    last_update_ns_ = 0;
    group_ = 0;
    prev_group_ = 0;
    reset_stats();
}

void CallbackGrouper::reset_stats() noexcept {
    min_group_.store(0, std::memory_order_relaxed);
    max_group_.store(0, std::memory_order_relaxed);
    callbacks_.store(0, std::memory_order_relaxed);
}

void AnchorSlot::publish(const ClockAnchor& anchor) noexcept {
    const uint64_t s = seq_.load(std::memory_order_relaxed);
    seq_.store(s + 1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    cursor_frames_.store(anchor.cursor_frames, std::memory_order_relaxed);
    timestamp_ns_.store(anchor.timestamp_ns, std::memory_order_relaxed);
    period_engine_frames_.store(anchor.period_engine_frames, std::memory_order_relaxed);
    seq_.store(s + 2, std::memory_order_release);
}

bool AnchorSlot::try_read(ClockAnchor& out) const noexcept {
    for (int attempt = 0; attempt < kMaxReadAttempts; ++attempt) {
        const uint64_t s1 = seq_.load(std::memory_order_acquire);
        if (s1 == 0) {
            return false; // never published
        }
        if ((s1 & 1U) != 0) {
            continue; // write in progress
        }
        ClockAnchor value;
        value.cursor_frames = cursor_frames_.load(std::memory_order_relaxed);
        value.timestamp_ns = timestamp_ns_.load(std::memory_order_relaxed);
        value.period_engine_frames = period_engine_frames_.load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        const uint64_t s2 = seq_.load(std::memory_order_relaxed);
        if (s1 == s2) {
            out = value;
            return true;
        }
    }
    return false;
}

} // namespace blaze4k
