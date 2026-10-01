#pragma once

#include <string>
#include <memory>
#include <cstdint>

struct ma_sound;

namespace blaze4k {

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
    [[nodiscard]] uint64_t get_position_frames() const;
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
};

} // namespace blaze4k
