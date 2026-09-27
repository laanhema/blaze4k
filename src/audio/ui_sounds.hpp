#pragma once

#include <filesystem>

namespace td {

// Menu UI sounds only (no gameplay hit SFX). Tundra presentation, unsourced.
enum class UiSound { Move, Confirm, Back };

// Injectable sink so the shell's trigger logic is testable without an audio
// device (mirrors `IAudioStream`/`Metronome`).
class IUiSoundSink {
public:
    virtual ~IUiSoundSink() = default;
    virtual void play(UiSound sound) = 0;
};

// Writes a short fixed-layout 16-bit mono PCM WAV for `sound` (pure I/O, no
// audio device required). Returns false on an empty path or write failure.
[[nodiscard]] bool write_ui_sound_wav(const std::filesystem::path& path, UiSound sound);

// Synthesizes the three menu WAVs under `<dir>/sfx/` (only when missing) and
// plays them as one-shots through the shared `AudioEngine`. Never throws; silent
// and non-blocking when no audio device is available (`init` returns false and
// `play` logs once).
class UiSoundPlayer : public IUiSoundSink {
public:
    UiSoundPlayer();
    ~UiSoundPlayer() override;

    UiSoundPlayer(const UiSoundPlayer&) = delete;
    UiSoundPlayer& operator=(const UiSoundPlayer&) = delete;

    bool init(const std::filesystem::path& dir);
    void play(UiSound sound) override;
    [[nodiscard]] bool is_ready() const { return ready_; }

private:
    [[nodiscard]] const std::filesystem::path& path_for(UiSound sound) const;

    std::filesystem::path paths_[3];
    bool ready_ = false;
    bool logged_unavailable_ = false;
};

} // namespace td
