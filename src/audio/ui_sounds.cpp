#include "audio/ui_sounds.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>

#include <miniaudio.h>

#include "audio/audio_engine.hpp"

namespace td {

namespace {

constexpr double kTwoPi = 6.28318530717958647692;
constexpr uint32_t kSampleRate = 44100;

// Move = short flat blip; Confirm = rising two-tone; Back = falling two-tone.
// Distinct durations also make the generated file sizes distinct.
struct Spec {
    double duration;
    double start_hz;
    double end_hz;
    double amplitude;
};

Spec spec_for(UiSound sound) {
    switch (sound) {
        case UiSound::Move: return {0.08, 880.0, 880.0, 0.35};
        case UiSound::Confirm: return {0.12, 660.0, 990.0, 0.40};
        case UiSound::Back: return {0.15, 660.0, 440.0, 0.40};
    }
    return {0.10, 440.0, 440.0, 0.30};
}

const char* file_name(UiSound sound) {
    switch (sound) {
        case UiSound::Move: return "move.wav";
        case UiSound::Confirm: return "confirm.wav";
        case UiSound::Back: return "back.wav";
    }
    return "ui.wav";
}

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

bool write_ui_sound_wav(const std::filesystem::path& path, UiSound sound) {
    if (path.empty()) {
        return false;
    }

    const Spec spec = spec_for(sound);
    if (spec.duration <= 0.0) {
        return false;
    }

    const auto total_samples =
        static_cast<uint32_t>(std::lround(spec.duration * static_cast<double>(kSampleRate)));
    if (total_samples == 0) {
        return false;
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    const uint16_t num_channels = 1;
    const uint16_t bits_per_sample = 16;
    const uint32_t data_size = total_samples * 2u;
    const uint32_t chunk_size = 36u + data_size;
    const uint32_t byte_rate =
        kSampleRate * static_cast<uint32_t>(num_channels) * (bits_per_sample / 8u);
    const uint16_t block_align =
        static_cast<uint16_t>(num_channels * (bits_per_sample / 8u));

    out.write("RIFF", 4);
    write_u32(out, chunk_size);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    write_u32(out, 16);
    write_u16(out, 1); // PCM
    write_u16(out, num_channels);
    write_u32(out, kSampleRate);
    write_u32(out, byte_rate);
    write_u16(out, block_align);
    write_u16(out, bits_per_sample);
    out.write("data", 4);
    write_u32(out, data_size);

    constexpr double kAttack = 0.005;
    double phase = 0.0;
    for (uint32_t i = 0; i < total_samples; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(kSampleRate);
        const double progress = t / spec.duration;
        const double freq = spec.start_hz + (spec.end_hz - spec.start_hz) * progress;
        phase += kTwoPi * freq / static_cast<double>(kSampleRate);

        double envelope = 0.0;
        if (t < kAttack) {
            envelope = t / kAttack;
        } else {
            envelope = std::max(0.0, 1.0 - (t - kAttack) / (spec.duration - kAttack));
        }

        const double value = spec.amplitude * envelope * std::sin(phase);
        const int16_t sample =
            static_cast<int16_t>(std::lround(std::clamp(value, -1.0, 1.0) * 32767.0));
        write_u16(out, static_cast<uint16_t>(sample));
    }

    out.flush();
    return out.good();
}

UiSoundPlayer::UiSoundPlayer() = default;
UiSoundPlayer::~UiSoundPlayer() = default;

const std::filesystem::path& UiSoundPlayer::path_for(UiSound sound) const {
    return paths_[static_cast<std::size_t>(sound)];
}

bool UiSoundPlayer::init(const std::filesystem::path& dir) {
    if (ready_) {
        return true;
    }
    if (dir.empty()) {
        std::cerr << "[UiSoundPlayer] No data directory; UI sounds disabled\n";
        ready_ = false;
        return false;
    }

    const std::filesystem::path sfx_dir = dir / "sfx";
    std::error_code ec;
    std::filesystem::create_directories(sfx_dir, ec);
    if (ec) {
        std::cerr << "[UiSoundPlayer] Cannot create '" << sfx_dir.string() << "': " << ec.message()
                  << "\n";
        ready_ = false;
        return false;
    }

    const UiSound sounds[3] = {UiSound::Move, UiSound::Confirm, UiSound::Back};
    for (UiSound sound : sounds) {
        std::filesystem::path path = sfx_dir / file_name(sound);
        std::error_code size_ec;
        const std::uintmax_t size = std::filesystem::file_size(path, size_ec);
        // A stat error (e.g. missing file) yields the (uintmax_t)-1 sentinel and a
        // set error code; treat it as absent/too small and (re)synthesize.
        if (size_ec || size < 44u) {
            if (!write_ui_sound_wav(path, sound)) {
                std::cerr << "[UiSoundPlayer] Failed to synthesize '" << path.string() << "'\n";
                ready_ = false;
                return false;
            }
        }
        paths_[static_cast<std::size_t>(sound)] = std::move(path);
    }

    AudioEngine& engine = AudioEngine::instance();
    if (!engine.is_initialized() && !engine.init()) {
        std::cerr << "[UiSoundPlayer] Audio engine unavailable; UI sounds silent\n";
        ready_ = false;
        return false;
    }

    ready_ = true;
    std::cout << "[UiSoundPlayer] UI sounds ready under '" << sfx_dir.string() << "'\n";
    return true;
}

void UiSoundPlayer::play(UiSound sound) {
    if (!ready_) {
        if (!logged_unavailable_) {
            std::cerr << "[UiSoundPlayer] UI sounds unavailable; input remains silent\n";
            logged_unavailable_ = true;
        }
        return;
    }

    AudioEngine& engine = AudioEngine::instance();
    if (engine.raw_engine() == nullptr) {
        return;
    }
    ma_engine_play_sound(engine.raw_engine(), path_for(sound).string().c_str(), nullptr);
}

} // namespace td
