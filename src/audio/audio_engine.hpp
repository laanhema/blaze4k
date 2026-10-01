#pragma once

#include <memory>
#include <string>

struct ma_engine;

namespace blaze4k {

class AudioEngine {
public:
    static AudioEngine& instance();

    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    bool init();
    void shutdown();

    [[nodiscard]] bool is_initialized() const { return initialized_; }
    void set_master_volume(float volume);
    [[nodiscard]] float get_master_volume() const;

    [[nodiscard]] ma_engine* raw_engine() { return engine_.get(); }

private:
    std::unique_ptr<ma_engine> engine_;
    bool initialized_ = false;
};

} // namespace blaze4k
