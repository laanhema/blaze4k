#pragma once

#include <string>

#include "audio/sound_stream.hpp"

namespace blaze4k {

// Delay between a highlight change and the preview starting (UI feel only; not
// sourced from OpenITG -- see plan OQ3).
inline constexpr double kPreviewDelaySeconds = 0.5;

enum class PreviewState { Idle, Waiting, Active };

// Owns a single looping song-preview stream. `request()` (called on highlight
// change) stops any current preview and arms a delayed load; `update()` advances
// the delay from the injected fixed_dt (never wall-clock) and, once active, seeks
// back to `start` when the `length` window has elapsed. Headless-safe: a failed
// `SoundStream::load` collapses to Idle without throwing.
class PreviewPlayer {
public:
    PreviewPlayer();

    // Test seam: drive the state machine with a caller-owned stream (e.g. a
    // fake) instead of the built-in SoundStream. The caller keeps ownership and
    // must outlive this player.
    explicit PreviewPlayer(IAudioStream& stream);

    // path empty -> stays Idle. Non-positive start/length are stored as 0,
    // meaning "from the beginning" and "no loop window".
    void request(const std::string& path, double start_seconds, double length_seconds);

    // Advances the delay timer / loop check. `fixed_dt` is the app's fixed step.
    void update(double fixed_dt);

    // Stops playback and resets to Idle (called on navigation and screen exit).
    void stop();

    void set_volume(float volume); // clamped to [0, 1]

    [[nodiscard]] PreviewState state() const { return state_; }
    [[nodiscard]] const std::string& requested_path() const { return path_; }
    [[nodiscard]] int load_attempts() const { return load_attempts_; }
    [[nodiscard]] double delay_seconds() const { return delay_seconds_; }
    [[nodiscard]] double start_seconds() const { return start_; }
    [[nodiscard]] double length_seconds() const { return length_; }

    void set_delay_seconds(double seconds) { delay_seconds_ = seconds > 0.0 ? seconds : 0.0; }

private:
    [[nodiscard]] IAudioStream& stream() {
        return stream_override_ != nullptr ? *stream_override_ : owned_stream_;
    }

    SoundStream owned_stream_;
    IAudioStream* stream_override_ = nullptr;
    PreviewState state_ = PreviewState::Idle;
    std::string path_;
    double start_ = 0.0;
    double length_ = 0.0;
    double timer_ = 0.0;
    double delay_seconds_ = kPreviewDelaySeconds;
    int load_attempts_ = 0;
    float volume_ = 1.0f;
};

} // namespace blaze4k
