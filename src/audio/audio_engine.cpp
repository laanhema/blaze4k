#include "audio/audio_engine.hpp"
#include <iostream>
#include <miniaudio.h>

namespace blaze4k {

AudioEngine& AudioEngine::instance() {
    static AudioEngine s_instance;
    return s_instance;
}

AudioEngine::AudioEngine()
    : engine_(std::make_unique<ma_engine>()) {}

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::init() {
    if (initialized_) {
        return true;
    }

    ma_engine_config config = ma_engine_config_init();
    ma_result result = ma_engine_init(&config, engine_.get());
    if (result != MA_SUCCESS) {
        std::cerr << "[AudioEngine] Warning: Audio device initialization failed ("
                  << static_cast<int>(result) << "). Running in silent fallback mode.\n";
        return false;
    }

    initialized_ = true;
    std::cout << "[AudioEngine] Initialized successfully. Sample rate: "
              << ma_engine_get_sample_rate(engine_.get()) << " Hz, Channels: "
              << ma_engine_get_channels(engine_.get()) << "\n";
    return true;
}

void AudioEngine::shutdown() {
    if (initialized_ && engine_) {
        ma_engine_uninit(engine_.get());
        initialized_ = false;
    }
}

void AudioEngine::set_master_volume(float volume) {
    if (initialized_ && engine_) {
        ma_engine_set_volume(engine_.get(), volume);
    }
}

float AudioEngine::get_master_volume() const {
    if (initialized_ && engine_) {
        return ma_engine_get_volume(engine_.get());
    }
    return 1.0f;
}

} // namespace blaze4k
