#include "audio/audio_engine.hpp"
#include <cstdio>
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

    // Diagnostic only (PRD: audio latency measurable via log output). The period is the miniaudio
    // client buffer; OS mixer, Bluetooth and headset latency are invisible here (docs/AUDIO_LATENCY.md).
    if (ma_device* device = ma_engine_get_device(engine_.get()); device != nullptr) {
        char name[MA_MAX_DEVICE_NAME_LENGTH + 1] = {};
        if (ma_device_get_name(device, ma_device_type_playback, name, sizeof(name), nullptr) !=
            MA_SUCCESS) {
            std::snprintf(name, sizeof(name), "%s", "<unknown>");
        }
        const ma_uint32 rate = device->playback.internalSampleRate;
        const ma_uint32 period = device->playback.internalPeriodSizeInFrames;
        const double period_ms = rate > 0 ? 1000.0 * period / rate : 0.0;
        std::cout << "[AudioEngine] Output device: '" << name << "' via "
                  << ma_get_backend_name(ma_device_get_context(device)->backend) << ", period " << period
                  << " frames x " << device->playback.internalPeriods << " @ " << rate
                  << " Hz (" << period_ms
                  << " ms/period; excludes OS mixer and Bluetooth latency)\n";
    }
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
