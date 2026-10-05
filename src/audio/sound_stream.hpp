#pragma once

#include <string>
#include <memory>
#include <cstdint>

#include "audio/clock_anchor.hpp"
#include "timing/clock_interpolator.hpp"

struct ma_sound;

namespace blaze4k {

// An interpolated cursor together with the monotonic ns (the engine's injected
// clock, SDL_GetTicksNS timebase in production) at which it is the estimate.
// timestamp_ns == 0: raw cursor, no timestamp.
struct TimedFrames {
    uint64_t frames = 0;
    uint64_t timestamp_ns = 0;
};

// Narrow playback surface that PreviewPlayer depends on. Lets tests drive the
// Waiting -> Active -> loop state machine with a fake implementation, so the
// success/seek-back path is covered without a real audio device.
class IAudioStream {
public:
    virtual ~IAudioStream() = default;

    virtual bool load(const std::string& filepath) = 0;
    virtual void stop() = 0;
    virtual bool play() = 0;
    virtual bool seek_seconds(double seconds) = 0;
    [[nodiscard]] virtual double get_position_seconds() const = 0;
    [[nodiscard]] virtual bool is_playing() const = 0;
    virtual void set_volume(float volume) = 0;
};

class SoundStream : public IAudioStream {
public:
    SoundStream();
    ~SoundStream() override;

    SoundStream(const SoundStream&) = delete;
    SoundStream& operator=(const SoundStream&) = delete;
    SoundStream(SoundStream&& other) noexcept;
    SoundStream& operator=(SoundStream&& other) noexcept;

    bool load(const std::string& filepath) override;
    void unload();

    bool play() override;
    void pause();
    void resume();
    void stop() override;

    bool seek_seconds(double seconds) override;
    [[nodiscard]] double get_position_seconds() const override;
    // Interpolated when enable_clock_interpolation() succeeded, raw otherwise.
    [[nodiscard]] uint64_t get_position_frames() const;
    [[nodiscard]] TimedFrames get_timed_position_frames() const;
    // The raw (callback-granular) miniaudio cursor, with the monotonic guard.
    [[nodiscard]] uint64_t get_raw_position_frames() const;
    [[nodiscard]] double get_raw_position_seconds() const;

    // #81: registers this stream's sound as the engine's clock tap so the
    // position is interpolated from audio-thread anchors. Call after load();
    // idempotent (re-attaching makes this the active tap again). False, and the
    // stream stays raw-only, when the engine is not initialized or has no
    // monotonic clock configured.
    bool enable_clock_interpolation();
    [[nodiscard]] bool clock_interpolation_enabled() const { return tap_attached_; }
    [[nodiscard]] uint32_t get_sample_rate() const { return sample_rate_; }
    [[nodiscard]] double get_length_seconds() const;
    [[nodiscard]] bool is_playing() const override;
    [[nodiscard]] bool is_loaded() const { return is_loaded_; }

    void set_volume(float volume) override;
    [[nodiscard]] float get_volume() const;

private:
    std::unique_ptr<ma_sound> sound_;
    bool is_loaded_ = false;
    uint32_t sample_rate_ = 44100;
    mutable double last_position_ = 0.0;

    void reset_interpolation(uint64_t floor_frames);

    std::unique_ptr<ClockTap> tap_;
    bool tap_attached_ = false;
    mutable ClockInterpolator interp_;
    uint32_t engine_rate_ = 0;
    uint64_t length_frames_ = 0;
};

} // namespace blaze4k
