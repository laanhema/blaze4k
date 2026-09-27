#include "audio/assist_tick_player.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <system_error>

#include <miniaudio.h>

#include "audio/audio_engine.hpp"

namespace td {

namespace {

constexpr double kTwoPi = 6.28318530717958647692;
constexpr uint32_t kSampleRate = 44100;
// Short, bright, Hann-windowed click: distinct from the 1 kHz calibration
// metronome and short enough for dense streams. Tundra presentation, unsourced.
constexpr double kTickHz = 2000.0;
constexpr double kTickSeconds = 0.03;
constexpr double kTickAmplitude = 0.6;
// Enough voices for the densest stream to overlap without cutting a tick short.
constexpr std::size_t kPoolSize = 16;

void write_u16(std::ofstream& out, uint16_t value) {
    const char bytes[2] = {
        static_cast<char>(value & 0xFFu),
        static_cast<char>((value >> 8) & 0xFFu),
    };
    out.write(bytes, 2);
}

void write_u32(std::ofstream& out, uint32_t value) {
    const char bytes[4] = {
        static_cast<char>(value & 0xFFu),
        static_cast<char>((value >> 8) & 0xFFu),
        static_cast<char>((value >> 16) & 0xFFu),
        static_cast<char>((value >> 24) & 0xFFu),
    };
    out.write(bytes, 4);
}

} // namespace

bool write_assist_tick_wav(const std::filesystem::path& path) {
    if (path.empty()) {
        return false;
    }

    const auto total_samples =
        static_cast<uint32_t>(std::lround(kTickSeconds * static_cast<double>(kSampleRate)));

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    const uint32_t data_size = total_samples * 2u;
    out.write("RIFF", 4);
    write_u32(out, 36u + data_size);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    write_u32(out, 16);
    write_u16(out, 1); // PCM
    write_u16(out, 1); // mono
    write_u32(out, kSampleRate);
    write_u32(out, kSampleRate * 2u); // byte rate
    write_u16(out, 2);                // block align
    write_u16(out, 16);               // bits per sample
    out.write("data", 4);
    write_u32(out, data_size);

    for (uint32_t i = 0; i < total_samples; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(kSampleRate);
        const double window = 0.5 * (1.0 - std::cos(kTwoPi * t / kTickSeconds));
        const double value = kTickAmplitude * window * std::sin(kTwoPi * kTickHz * t);
        const auto sample =
            static_cast<int16_t>(std::lround(std::clamp(value, -1.0, 1.0) * 32767.0));
        write_u16(out, static_cast<uint16_t>(sample));
    }

    out.flush();
    return out.good();
}

AssistTickPlayer::AssistTickPlayer() = default;

AssistTickPlayer::~AssistTickPlayer() {
    shutdown();
}

bool AssistTickPlayer::init(const std::filesystem::path& wav_path) {
    shutdown();
    if (wav_path.empty()) {
        std::cerr << "[AssistTick] No sound path; assist tick disabled\n";
        return false;
    }

    std::error_code ec;
    if (!wav_path.parent_path().empty()) {
        std::filesystem::create_directories(wav_path.parent_path(), ec);
    }
    std::error_code size_ec;
    const std::uintmax_t size = std::filesystem::file_size(wav_path, size_ec);
    if ((size_ec || size < 44u) && !write_assist_tick_wav(wav_path)) {
        std::cerr << "[AssistTick] Failed to synthesize '" << wav_path.string() << "'\n";
        return false;
    }

    AudioEngine& engine = AudioEngine::instance();
    if (!engine.is_initialized() && !engine.init()) {
        std::cerr << "[AssistTick] Audio engine unavailable; assist tick silent\n";
        return false;
    }

    // MA_SOUND_FLAG_DECODE: the resource manager decodes the file once and every
    // pool voice shares that buffer.
    for (std::size_t i = 0; i < kPoolSize; ++i) {
        auto sound = std::make_unique<ma_sound>();
        if (ma_sound_init_from_file(engine.raw_engine(), wav_path.string().c_str(),
                                    MA_SOUND_FLAG_DECODE, nullptr, nullptr,
                                    sound.get()) != MA_SUCCESS) {
            std::cerr << "[AssistTick] Failed to load '" << wav_path.string() << "'\n";
            shutdown();
            return false;
        }
        pool_.push_back(std::move(sound));
    }

    next_ = 0;
    ready_ = true;
    return true;
}

void AssistTickPlayer::play_in(double seconds_from_now) {
    if (!ready_) {
        return;
    }
    ma_engine* engine = AudioEngine::instance().raw_engine();
    ma_sound* sound = pool_[next_].get();
    next_ = (next_ + 1) % pool_.size();

    ma_sound_stop(sound);
    ma_sound_seek_to_pcm_frame(sound, 0);
    const double rate = static_cast<double>(ma_engine_get_sample_rate(engine));
    const auto delay_frames =
        static_cast<ma_uint64>(std::llround(std::max(0.0, seconds_from_now) * rate));
    ma_sound_set_start_time_in_pcm_frames(sound,
                                          ma_engine_get_time_in_pcm_frames(engine) + delay_frames);
    ma_sound_start(sound);
}

void AssistTickPlayer::stop_all() {
    for (const std::unique_ptr<ma_sound>& sound : pool_) {
        ma_sound_stop(sound.get());
    }
}

void AssistTickPlayer::shutdown() {
    for (const std::unique_ptr<ma_sound>& sound : pool_) {
        ma_sound_uninit(sound.get());
    }
    pool_.clear();
    next_ = 0;
    ready_ = false;
}

} // namespace td
