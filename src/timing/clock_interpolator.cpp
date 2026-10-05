#include "timing/clock_interpolator.hpp"

#include <algorithm>

namespace blaze4k {

void ClockInterpolator::reset(uint64_t now_ns, uint64_t floor_frames) {
    reset_ns_ = now_ns;
    floor_ = floor_frames;
}

uint64_t ClockInterpolator::update(const InterpolationInput& in) {
    const uint64_t raw = in.raw_cursor_frames;
    if (!in.playing) {
        floor_ = raw;
        return raw;
    }

    uint64_t est = raw;
    if (in.anchor_valid && in.anchor.timestamp_ns >= reset_ns_ && in.source_rate > 0 &&
        in.engine_rate > 0) {
        const uint64_t elapsed =
            in.now_ns > in.anchor.timestamp_ns ? in.now_ns - in.anchor.timestamp_ns : 0;
        // long double keeps elapsed * rate exact enough without overflowing
        // uint64 after a multi-second stall.
        const auto lead = static_cast<uint64_t>(static_cast<long double>(elapsed) *
                                                static_cast<long double>(in.source_rate) / 1e9L);
        const uint64_t cap = static_cast<uint64_t>(in.anchor.period_engine_frames) *
                             static_cast<uint64_t>(in.source_rate) /
                             static_cast<uint64_t>(in.engine_rate);
        est = std::max(raw, in.anchor.cursor_frames + std::min(lead, cap));
    }

    if (in.length_frames > 0) {
        est = std::max(raw, std::min(est, in.length_frames));
    }

    const uint64_t out = std::max(est, floor_);
    floor_ = out;
    return out;
}

} // namespace blaze4k
