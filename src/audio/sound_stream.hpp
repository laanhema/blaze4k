#pragma once

#include <string>
#include <memory>
#include <cstdint>

struct ma_sound;

namespace td {

class SoundStream {
public:
    SoundStream();
    ~SoundStream();

    SoundStream(const SoundStream&) = delete;
    SoundStream& operator=(const SoundStream&) = delete;
    SoundStream(SoundStream&& other) noexcept;
    SoundStream& operator=(SoundStream&& other) noexcept;

    bool load(const std::string& filepath);
    void unload();

    bool play();
    void pause();
    void resume();
    void stop();

    bool seek_seconds(double seconds);
    [[nodiscard]] double get_position_seconds() const;
    [[nodiscard]] uint64_t get_position_frames() const;
    [[nodiscard]] uint32_t get_sample_rate() const { return sample_rate_; }
    [[nodiscard]] double get_length_seconds() const;
    [[nodiscard]] bool is_playing() const;
    [[nodiscard]] bool is_loaded() const { return is_loaded_; }

    void set_volume(float volume);
    [[nodiscard]] float get_volume() const;

private:
    std::unique_ptr<ma_sound> sound_;
    bool is_loaded_ = false;
    uint32_t sample_rate_ = 44100;
    mutable double last_position_ = 0.0;
};

} // namespace td
