#pragma once

#include <cstdint>

namespace td {

// Reconstructs the music-clock time at which an SDL-timestamped input occurred,
// given the music time and SDL nanosecond reference sampled together. Mirrors
// OpenITG's `fMusicSeconds = fCurrentMusicSeconds - fTimeSinceStep`
// (src/Player.cpp:908-919) but is a pure function: it reads no clock.
[[nodiscard]] inline double music_time_for_event(
    uint64_t event_timestamp_ns, uint64_t reference_ns, double reference_music_seconds) {
    if (event_timestamp_ns == 0 || event_timestamp_ns >= reference_ns) {
        return reference_music_seconds;   // unset / future event: no age
    }
    const double age = static_cast<double>(reference_ns - event_timestamp_ns) / 1e9;
    return reference_music_seconds - age;
}

} // namespace td
