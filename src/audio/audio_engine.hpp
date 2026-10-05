#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include "audio/clock_anchor.hpp"

struct ma_engine;
struct ma_context;

namespace blaze4k {

// #81 (B from #71): engine period request. 480 frames = 10 ms @ 48 kHz. 0 means
// the miniaudio/backend default. Overridable via config `audio.period_size_frames`.
inline constexpr uint32_t kDefaultAudioPeriodFrames = 480;

// Injected monotonic clock (SDL_GetTicksNS in production). A plain function
// pointer keeps src/audio SDL-free; it is called on the audio thread.
using MonotonicNowFn = uint64_t (*)();

struct AudioEngineSettings {
    uint32_t period_size_frames = kDefaultAudioPeriodFrames;
    MonotonicNowFn now_ns = nullptr; // nullptr: no clock interpolation (raw cursor only)
    // Headless tests and the clock probe only: a silent, real-time-paced
    // miniaudio null device. Never set by main.cpp.
    bool use_null_backend = false;
};

class AudioEngine {
public:
    struct CallbackStats {
        uint32_t min_frames = 0; // device-callback interval, engine frames (0 = none yet)
        uint32_t max_frames = 0;
        uint64_t callbacks = 0;
        uint32_t sample_rate = 0;
    };

    static AudioEngine& instance();

    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    // Must run before init(). If already initialized, the settings are stored
    // and apply on the next init (a warning is logged).
    void configure(const AudioEngineSettings& settings);
    [[nodiscard]] const AudioEngineSettings& settings() const { return settings_; }

    bool init();
    void shutdown();

    [[nodiscard]] bool is_initialized() const { return initialized_; }
    void set_master_volume(float volume);
    [[nodiscard]] float get_master_volume() const;

    [[nodiscard]] ma_engine* raw_engine() { return engine_.get(); }
    [[nodiscard]] uint32_t engine_sample_rate() const { return engine_rate_; }
    [[nodiscard]] MonotonicNowFn now_fn() const { return settings_.now_ns; }

    // One active clock tap. attach returns false when the engine is not
    // initialized or no monotonic clock was configured; it replaces any
    // previous tap. detach is a no-op unless `tap` is the active one; after it
    // returns, the audio thread no longer touches the tap's sound.
    bool attach_clock_tap(ClockTap* tap);
    void detach_clock_tap(ClockTap* tap);

    [[nodiscard]] CallbackStats callback_stats() const;
    void log_callback_stats(const char* context) const;

    // Audio thread (miniaudio onProcess). Lock-, allocation- and logging-free.
    void on_process(uint64_t frames) noexcept;

private:
    std::unique_ptr<ma_engine> engine_;
    std::unique_ptr<ma_context> null_context_;
    bool initialized_ = false;
    AudioEngineSettings settings_;
    MonotonicNowFn now_ns_ = nullptr; // snapshot used by the audio thread
    uint32_t engine_rate_ = 0;
    CallbackGrouper grouper_;
    std::atomic<ClockTap*> active_tap_{nullptr};
    std::atomic<bool> in_process_{false};
};

} // namespace blaze4k
