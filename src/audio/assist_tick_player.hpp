#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <vector>

struct ma_sound;

namespace td {

// Writes the short assist-tick click as a fixed-layout 16-bit mono PCM WAV (pure
// I/O, no audio device). Returns false on an empty path or write failure.
[[nodiscard]] bool write_assist_tick_wav(const std::filesystem::path& path);

// Plays the assist tick through the shared AudioEngine with sample-accurate
// start times on the engine clock (StepMania schedules its tick the same way via
// RageSoundParams::m_StartTime), so a tick requested ahead of time lands on the
// note regardless of frame timing. A small round-robin pool lets consecutive
// ticks overlap. Silent no-op when no audio device is available.
class AssistTickPlayer {
public:
    AssistTickPlayer();
    ~AssistTickPlayer();

    AssistTickPlayer(const AssistTickPlayer&) = delete;
    AssistTickPlayer& operator=(const AssistTickPlayer&) = delete;

    // Synthesizes `wav_path` when missing, then loads the pool. Never throws.
    bool init(const std::filesystem::path& wav_path);
    // Starts one tick `seconds_from_now` on the engine clock (<= 0 = immediately).
    void play_in(double seconds_from_now);
    void stop_all(); // cancels scheduled and sounding ticks
    void shutdown();

    [[nodiscard]] bool is_ready() const { return ready_; }

private:
    std::vector<std::unique_ptr<ma_sound>> pool_;
    std::size_t next_ = 0;
    bool ready_ = false;
};

} // namespace td
