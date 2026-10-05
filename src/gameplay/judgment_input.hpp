#pragma once

#include <cstdint>

namespace blaze4k {

// Reconstructs the music-clock time at which an SDL-timestamped input occurred,
// given the music time and SDL nanosecond reference sampled together. Mirrors
// OpenITG's `fMusicSeconds = fCurrentMusicSeconds - fTimeSinceStep`
// (src/Player.cpp:918-926 @ f2c129fe65c65e4a9b3a691ff35e7717b4e8de51) but is a
// pure function: it reads no clock.
[[nodiscard]] inline double music_time_for_event(
    uint64_t event_timestamp_ns, uint64_t reference_ns, double reference_music_seconds) {
    if (event_timestamp_ns == 0 || event_timestamp_ns >= reference_ns) {
        return reference_music_seconds;   // unset / future event: no age
    }
    const double age = static_cast<double>(reference_ns - event_timestamp_ns) / 1e9;
    return reference_music_seconds - age;
}

// #81: the aging reference is the timestamp the music time was estimated at
// (MusicClock::timed_time_seconds), so position and "now" form one consistent
// pair. Sources without a timestamp (0: stub/injected/raw) fall back to the
// caller's SDL reference sampled after the event drain.
[[nodiscard]] inline uint64_t aging_reference_ns(uint64_t clock_timestamp_ns, uint64_t fallback_ns) {
    return clock_timestamp_ns != 0 ? clock_timestamp_ns : fallback_ns;
}

} // namespace blaze4k
