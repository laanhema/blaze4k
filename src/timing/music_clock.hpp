#pragma once

#include <cstdint>
#include <functional>

namespace td {

// Raw audio position: number of PCM frames consumed by the stream cursor.
struct SamplePosition {
    uint64_t frames = 0;
    uint32_t sample_rate = 48000;
};

// Gameplay clock derived exclusively from the audio stream's PCM frame cursor.
//
// Formula (PRD section 6 pattern 1 / issue AC):
//   time_seconds = frames / sample_rate + global_offset_seconds
//
// Sign convention: a positive global offset makes the clock read later
// (positive offset = clock later). Values are stored in Tundra's own sign;
// they are NOT mirrored from StepMania's `- offset` convention.
//
// Production binding (one-line adapter over a SoundStream):
//   clock.set_source([&]{ return SamplePosition{s.get_position_frames(), s.get_sample_rate()}; });
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

    static double seconds_from_pcm(uint64_t frames, uint32_t sample_rate);
    static double apply_offset(double sample_seconds, double offset_seconds);

private:
    Source source_;
    double global_offset_seconds_ = 0.0;
};

} // namespace td
