#include "audio/audio_engine.hpp"
#include <cstdio>
#include <iostream>
#include <thread>
#include <miniaudio.h>

namespace blaze4k {

namespace {

// miniaudio `ma_engine_process_proc` (miniaudio.h:11172): fired at the end of
// every ma_engine_read_pcm_frames, on the audio thread.
void on_process_thunk(void* user_data, float* frames_out, ma_uint64 frame_count) {
    (void)frames_out;
    if (user_data != nullptr) {
        static_cast<AudioEngine*>(user_data)->on_process(static_cast<uint64_t>(frame_count));
    }
}

} // namespace

AudioEngine& AudioEngine::instance() {
    static AudioEngine s_instance;
    return s_instance;
}

AudioEngine::AudioEngine()
    : engine_(std::make_unique<ma_engine>()) {}

AudioEngine::~AudioEngine() {
    shutdown();
}

void AudioEngine::configure(const AudioEngineSettings& settings) {
    if (initialized_) {
        std::cerr << "[AudioEngine] configure() after init; period applies on next init\n";
    }
    settings_ = settings;
    if (settings_.now_ns != nullptr) {
        // Prime any lazy clock initialization (SDL ticks) on this thread before
        // the audio thread first calls it.
        (void)settings_.now_ns();
    }
}

bool AudioEngine::init() {
    if (initialized_) {
        return true;
    }

    ma_engine_config config = ma_engine_config_init();
    config.periodSizeInFrames = settings_.period_size_frames;
    config.onProcess = &on_process_thunk;
    config.pProcessUserData = this;

    if (settings_.use_null_backend) {
        null_context_ = std::make_unique<ma_context>();
        const ma_backend backends[] = {ma_backend_null};
        if (ma_context_init(backends, 1, nullptr, null_context_.get()) != MA_SUCCESS) {
            std::cerr << "[AudioEngine] Warning: null backend context initialization failed\n";
            null_context_.reset();
            return false;
        }
        config.pContext = null_context_.get();
    }

    // Set before the device starts so the audio thread sees a consistent snapshot.
    now_ns_ = settings_.now_ns;
    grouper_.reset();
    stats_reset_requested_.store(false, std::memory_order_relaxed);
    active_tap_.store(nullptr, std::memory_order_seq_cst);

    ma_result result = ma_engine_init(&config, engine_.get());
    if (result != MA_SUCCESS) {
        std::cerr << "[AudioEngine] Warning: Audio device initialization failed ("
                  << static_cast<int>(result) << "). Running in silent fallback mode.\n";
        if (null_context_) {
            ma_context_uninit(null_context_.get());
            null_context_.reset();
        }
        now_ns_ = nullptr;
        return false;
    }

    initialized_ = true;
    engine_rate_.store(ma_engine_get_sample_rate(engine_.get()), std::memory_order_relaxed);
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
                  << ma_get_backend_name(ma_device_get_context(device)->backend)
                  << ", requested period " << settings_.period_size_frames
                  << " frames (0 = backend default), period " << period
                  << " frames x " << device->playback.internalPeriods << " @ " << rate
                  << " Hz (" << period_ms
                  << " ms/period; excludes OS mixer and Bluetooth latency)\n";
    }
    return true;
}

void AudioEngine::shutdown() {
    active_tap_.store(nullptr, std::memory_order_seq_cst);
    if (initialized_ && engine_) {
        ma_engine_uninit(engine_.get());
        initialized_ = false;
    }
    if (null_context_) {
        ma_context_uninit(null_context_.get());
        null_context_.reset();
    }
    now_ns_ = nullptr;
    engine_rate_.store(0, std::memory_order_relaxed);
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

bool AudioEngine::attach_clock_tap(ClockTap* tap) {
    if (tap == nullptr || !initialized_ || now_ns_ == nullptr) {
        return false;
    }
    ClockTap* const previous = active_tap_.exchange(tap, std::memory_order_seq_cst);
    if (previous != nullptr && previous != tap) {
        // Same handshake as detach: the caller may free `previous->sound` next.
        wait_for_audio_quiescence();
    }
    return true;
}

void AudioEngine::detach_clock_tap(ClockTap* tap) {
    if (tap == nullptr || active_tap_.load(std::memory_order_seq_cst) != tap) {
        return;
    }
    // Dekker-style handshake with on_process (both sides seq_cst): once the tap
    // is cleared and no process call is in flight, the audio thread can no
    // longer reach `tap->sound`.
    active_tap_.store(nullptr, std::memory_order_seq_cst);
    wait_for_audio_quiescence();
}

void AudioEngine::wait_for_audio_quiescence() const {
    while (in_process_.load(std::memory_order_seq_cst)) {
        std::this_thread::yield();
    }
}

void AudioEngine::on_process(uint64_t frames) noexcept {
    in_process_.store(true, std::memory_order_seq_cst);
    if (frames > 0 && now_ns_ != nullptr) {
        // Time BEFORE the cursor: pre-seek cursors always carry a timestamp
        // earlier than the game thread's post-seek reset time.
        const uint64_t ns = now_ns_();
        // Zero first, then publish "done" (release): a reader that sees false
        // also sees the zeroed stats. A request landing in between is absorbed
        // by this just-applied reset.
        if (stats_reset_requested_.load(std::memory_order_acquire)) {
            grouper_.reset_stats();
            stats_reset_requested_.store(false, std::memory_order_release);
        }
        const uint32_t period =
            grouper_.on_update(frames, ns, engine_rate_.load(std::memory_order_relaxed));
        ClockTap* tap = active_tap_.load(std::memory_order_seq_cst);
        ma_sound* sound = tap != nullptr ? tap->sound.load(std::memory_order_acquire) : nullptr;
        if (sound != nullptr) {
            ma_uint64 cursor = 0;
            if (ma_sound_get_cursor_in_pcm_frames(sound, &cursor) == MA_SUCCESS) {
                tap->slot.publish(ClockAnchor{static_cast<uint64_t>(cursor), ns, period});
            }
        }
    }
    in_process_.store(false, std::memory_order_release);
}

void AudioEngine::reset_callback_stats() {
    stats_reset_requested_.store(true, std::memory_order_release);
}

AudioEngine::CallbackStats AudioEngine::callback_stats() const {
    const uint32_t rate = engine_rate_.load(std::memory_order_relaxed);
    if (stats_reset_requested_.load(std::memory_order_acquire)) {
        return CallbackStats{0, 0, 0, rate}; // reset not yet applied by the audio thread
    }
    const CallbackGrouper::Stats s = grouper_.stats();
    return CallbackStats{s.min_group, s.max_group, s.callbacks, rate};
}

void AudioEngine::log_callback_stats(const char* context) const {
    const CallbackStats s = callback_stats();
    std::cout << "[AudioEngine] Device callback interval (" << (context != nullptr ? context : "")
              << "): min " << s.min_frames << " / max " << s.max_frames << " frames over "
              << s.callbacks << " callbacks @ " << s.sample_rate << " Hz\n";
}

} // namespace blaze4k
