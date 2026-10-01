#pragma once

#include <cstdint>
#include <filesystem>

#include "audio/sound_stream.hpp"
#include "timing/music_clock.hpp"

namespace blaze4k {

// Steady-metronome parameters shared by the click-track generator and the
// wizard's beat schedule. Defaults match CalibrationConfig (120 BPM, 2 s lead-in,
// 64 beats) so the audible click and the analytic beat schedule line up (OQ3).
struct MetronomeConfig {
    double bpm = 120.0;
    double lead_in_seconds = 2.0;
    int beats = 64;
    double click_frequency_hz = 1000.0;
    double click_duration_seconds = 0.04;
    double click_amplitude = 0.7;
    double sample_rate = 44100.0;

    [[nodiscard]] double beat_period_seconds() const { return 60.0 / bpm; }
};

// Synthesizes a 16-bit mono PCM WAV click track with one short windowed sine
// click at each scheduled beat (mirrors tests/test_wav_writer.hpp). Returns false
// on any I/O or parameter failure; never throws.
[[nodiscard]] bool write_click_track(const std::filesystem::path& path,
                                     const MetronomeConfig& config);

// Owns the wizard's click stream and exposes it as a MusicClock::Source so the
// screen's clock is bound to the real stream position exactly as gameplay is.
// `Metronome(IAudioStream&)` is the test seam (mirrors PreviewPlayer); a failed
// `prepare()` leaves `using_stub() == true` so the caller can refuse to save.
class Metronome {
public:
    Metronome();
    explicit Metronome(IAudioStream& stream);

    bool prepare(const std::filesystem::path& wav_path, const MetronomeConfig& config = {});
    void start();
    void stop();

    [[nodiscard]] MusicClock::Source clock_source();
    [[nodiscard]] const MetronomeConfig& config() const { return config_; }
    [[nodiscard]] bool is_playing() const { return playing_; }
    [[nodiscard]] bool using_stub() const { return using_stub_; }

private:
    [[nodiscard]] IAudioStream& stream();

    SoundStream owned_;
    IAudioStream* override_ = nullptr;
    MetronomeConfig config_{};
    bool prepared_ = false;
    bool playing_ = false;
    bool using_stub_ = false;
};

} // namespace blaze4k
