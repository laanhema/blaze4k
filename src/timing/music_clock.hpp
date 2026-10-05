#pragma once

#include <cstdint>
#include <functional>

namespace blaze4k {

// Audio position: number of PCM frames consumed by the stream cursor (or the
// callback-anchored interpolated estimate of it, #81). `timestamp_ns` is the
// monotonic ns (SDL_GetTicksNS timebase) at which `frames` is the estimate;
// 0 means unknown (stub/injected sources, raw cursor).
struct SamplePosition {
    uint64_t frames = 0;
    uint32_t sample_rate = 48000;
    uint64_t timestamp_ns = 0;
};

// Offset-applied music time together with the timestamp of the one source
// sample it came from: the consistent (position, timestamp) pair input aging
// uses (OpenITG RageSound.cpp:742-756; SM5 RageSoundDriver_Generic_Software).
struct TimedMusicTime {
    double seconds = 0.0;
    uint64_t timestamp_ns = 0;
};

// Gameplay clock derived exclusively from the audio stream's PCM frame cursor.
//
// Formula (PRD section 6 pattern 1 / issue AC):
//   time_seconds = frames / sample_rate + global_offset_seconds
//
// Sign convention: a positive global offset makes the clock read later
// (positive offset = clock later). Values are stored in Blaze 4k's own sign;
// they are NOT mirrored from StepMania's `- offset` convention.
//
// Production binding (adapter over a SoundStream with clock interpolation
// enabled, #81):
//   clock.set_source([&]{
//       const TimedFrames p = s.get_timed_position_frames();
//       return SamplePosition{p.frames, s.get_sample_rate(), p.timestamp_ns};
//   });
//
// This module must remain free of wall-clock/frame-timing inputs: it includes
// only <cstdint> and <functional>, with no platform or timing headers.
class MusicClock {
public:
    using Source = std::function<SamplePosition()>;

    MusicClock() = default;
    explicit MusicClock(Source source);

    void set_source(Source source);
    void clear_source();
    [[nodiscard]] bool has_source() const;

    // Rejects non-finite input by keeping the last finite offset and logging a warning.
    void set_global_offset_seconds(double offset);
    [[nodiscard]] double global_offset_seconds() const;

    [[nodiscard]] SamplePosition sample_position() const;
    [[nodiscard]] double sample_time_seconds() const; // frames / rate
    [[nodiscard]] double time_seconds() const;        // frames / rate + offset
    [[nodiscard]] int64_t time_nanoseconds() const;   // round(time_seconds * 1e9)
    // Samples the source once: time_seconds() plus that sample's timestamp_ns.
    [[nodiscard]] TimedMusicTime timed_time_seconds() const;

    static double seconds_from_pcm(uint64_t frames, uint32_t sample_rate);
    static double apply_offset(double sample_seconds, double offset_seconds);

private:
    Source source_;
    double global_offset_seconds_ = 0.0;
};

} // namespace blaze4k
