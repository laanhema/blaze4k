#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "audio/metronome.hpp"
#include "screens/screen.hpp"
#include "timing/music_clock.hpp"
#include "timing/offset_calibration.hpp"

namespace td {

enum class CalibrationPhase : int { CountIn = 0, Sampling, Ready };

// C5 guided tap-to-the-beat calibration wizard (PRD section 7.5). Owns a real
// MusicClock bound to the metronome stream exactly as GameplayView binds its
// clock, ages each panel press through the same `music_time_for_event()` helper
// gameplay uses, and derives the global offset via the pure OffsetCalibration
// model. Confirm writes `ctx.config->offset.global_offset_seconds`; Back (the
// manager's default Calibration -> Select abort) never writes the config.
//
// Constructible and updatable headless: the test-seam ctor injects an
// IAudioStream + MusicClock::Source, and the production path falls back to a
// fixed_dt-advanced synthetic clock that refuses to save when no audio is
// available.
class CalibrationScreen : public Screen {
public:
    CalibrationScreen();
    explicit CalibrationScreen(std::filesystem::path click_wav_path);

    // Test seam. `source` drives the clock directly (no device); when empty, the
    // supplied `stream` is used by the metronome, so a fake failing stream
    // exercises the synthetic no-audio path.
    CalibrationScreen(IAudioStream& stream, MusicClock::Source source,
                      CalibrationConfig config = {});

    [[nodiscard]] ScreenId id() const override { return ScreenId::Calibration; }
    void enter(ScreenContext& ctx) override;
    void update(ScreenContext& ctx, double fixed_dt, const std::vector<InputEvent>& events) override;
    void render(ScreenContext& ctx, GlQuadRenderer& renderer, int w, int h) override;
    void exit(ScreenContext& ctx) override;

    // Test accessors (headless, pure).
    [[nodiscard]] CalibrationPhase phase() const { return phase_; }
    [[nodiscard]] int sample_count() const { return calib_.sample_count(); }
    [[nodiscard]] const CalibrationResult& result() const { return result_; }
    [[nodiscard]] bool audio_available() const { return !synthetic_; }
    [[nodiscard]] bool saved() const { return saved_; }

private:
    MusicClock clock_;
    Metronome metronome_;
    OffsetCalibration calib_;
    CalibrationConfig config_{};
    CalibrationResult result_{};
    std::filesystem::path click_path_;
    MusicClock::Source injected_source_{};
    IAudioStream* injected_stream_ = nullptr;
    bool source_injected_ = false;
    bool synthetic_ = false;
    bool saved_ = false;
    CalibrationPhase phase_ = CalibrationPhase::CountIn;
    double stub_frames_ = 0.0;
    uint32_t stub_rate_ = 44100;
};

} // namespace td
