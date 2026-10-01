#include "audio/metronome.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <system_error>

namespace blaze4k {

namespace {

constexpr double kTwoPi = 6.28318530717958647692;

void write_u16(std::ofstream& out, uint16_t value) {
    out.write(reinterpret_cast<const char*>(&value), 2);
}

void write_u32(std::ofstream& out, uint32_t value) {
    out.write(reinterpret_cast<const char*>(&value), 4);
}

// True when `path` already holds a click track generated for this exact config.
// The WAV is fixed-layout 16-bit mono PCM (see write_click_track): data_size at
// offset 40 encodes the total sample count, which depends on sample rate, BPM,
// lead-in, beat count, and click duration, so a size + sample-rate match pins
// every audible parameter we write.
bool click_track_matches(const std::filesystem::path& path, const MetronomeConfig& config) {
    if (config.beats <= 0 || config.bpm <= 0.0 || config.sample_rate <= 0.0 ||
        config.click_duration_seconds <= 0.0) {
        return false;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }

    char riff[4] = {};
    char wave[4] = {};
    uint32_t sample_rate = 0;
    uint32_t data_size = 0;
    in.read(riff, 4);
    in.seekg(8);
    in.read(wave, 4);
    in.seekg(24);
    in.read(reinterpret_cast<char*>(&sample_rate), 4);
    in.seekg(40);
    in.read(reinterpret_cast<char*>(&data_size), 4);
    if (!in.good() || std::string(riff, 4) != "RIFF" || std::string(wave, 4) != "WAVE") {
        return false;
    }

    const double period = config.beat_period_seconds();
    const double end_time = config.lead_in_seconds +
                            static_cast<double>(config.beats - 1) * period +
                            config.click_duration_seconds;
    const uint32_t expected_rate = static_cast<uint32_t>(config.sample_rate + 0.5);
    const uint64_t expected_samples =
        static_cast<uint64_t>(end_time * static_cast<double>(expected_rate)) + 1;
    const uint32_t expected_data_size = static_cast<uint32_t>(expected_samples * 2);
    return sample_rate == expected_rate && data_size == expected_data_size;
}

} // namespace

bool write_click_track(const std::filesystem::path& path, const MetronomeConfig& config) {
    if (path.empty() || config.beats <= 0 || config.bpm <= 0.0 || config.sample_rate <= 0.0 ||
        config.click_duration_seconds <= 0.0) {
        return false;
    }

    const double period = config.beat_period_seconds();
    const double end_time = config.lead_in_seconds +
                            static_cast<double>(config.beats - 1) * period +
                            config.click_duration_seconds;
    const uint32_t sample_rate = static_cast<uint32_t>(config.sample_rate + 0.5);
    if (sample_rate == 0) {
        return false;
    }
    const uint64_t total_samples =
        static_cast<uint64_t>(end_time * static_cast<double>(sample_rate)) + 1;

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    const uint16_t num_channels = 1;
    const uint16_t bits_per_sample = 16;
    const uint32_t data_size = static_cast<uint32_t>(total_samples * 2);
    const uint32_t chunk_size = 36 + data_size;
    const uint32_t byte_rate =
        sample_rate * static_cast<uint32_t>(num_channels) * (bits_per_sample / 8);
    const uint16_t block_align = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));

    out.write("RIFF", 4);
    write_u32(out, chunk_size);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    write_u32(out, 16);
    write_u16(out, 1); // PCM
    write_u16(out, num_channels);
    write_u32(out, sample_rate);
    write_u32(out, byte_rate);
    write_u16(out, block_align);
    write_u16(out, bits_per_sample);
    out.write("data", 4);
    write_u32(out, data_size);

    for (uint64_t i = 0; i < total_samples; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(sample_rate);
        double value = 0.0;

        const double relative = t - config.lead_in_seconds;
        const long beat_index = std::lround(relative / period);
        if (beat_index >= 0 && beat_index < config.beats) {
            const double beat_time =
                config.lead_in_seconds + static_cast<double>(beat_index) * period;
            const double local = t - beat_time;
            if (local >= 0.0 && local < config.click_duration_seconds) {
                const double phase = local / config.click_duration_seconds;
                const double window = 0.5 * (1.0 - std::cos(kTwoPi * phase));
                value = config.click_amplitude * window *
                        std::sin(kTwoPi * config.click_frequency_hz * local);
            }
        }

        const int16_t sample = static_cast<int16_t>(
            std::lround(std::clamp(value, -1.0, 1.0) * 32767.0));
        out.write(reinterpret_cast<const char*>(&sample), 2);
    }

    out.flush();
    return out.good();
}

Metronome::Metronome() = default;

Metronome::Metronome(IAudioStream& stream) : override_(&stream) {}

IAudioStream& Metronome::stream() {
    return override_ != nullptr ? *override_ : static_cast<IAudioStream&>(owned_);
}

bool Metronome::prepare(const std::filesystem::path& wav_path, const MetronomeConfig& config) {
    config_ = config;
    playing_ = false;

    if (wav_path.empty()) {
        prepared_ = false;
        using_stub_ = true;
        return false;
    }

    const std::filesystem::path parent = wav_path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
    }

    std::error_code exists_ec;
    const bool exists = std::filesystem::exists(wav_path, exists_ec);
    if (!exists || !click_track_matches(wav_path, config_)) {
        // Best-effort (re)generation: a reused file from a different config would
        // desync the audible click from the analytic beat schedule. stream().load()
        // below reports failure if the file is still unusable.
        const bool wrote = write_click_track(wav_path, config_);
        (void)wrote;
    }

    const bool ok = stream().load(wav_path.string());
    prepared_ = ok;
    using_stub_ = !ok;
    return ok;
}

void Metronome::start() {
    if (!prepared_) {
        return;
    }
    playing_ = stream().play();
}

void Metronome::stop() {
    stream().stop();
    playing_ = false;
}

MusicClock::Source Metronome::clock_source() {
    if (override_ != nullptr) {
        IAudioStream* raw = override_;
        const double rate = config_.sample_rate > 0.0 ? config_.sample_rate : 44100.0;
        return [raw, rate] {
            const double position = raw->get_position_seconds();
            const uint64_t frames =
                position > 0.0 ? static_cast<uint64_t>(std::llround(position * rate)) : 0;
            return SamplePosition{frames, static_cast<uint32_t>(rate)};
        };
    }
    return [this] {
        return SamplePosition{owned_.get_position_frames(), owned_.get_sample_rate()};
    };
}

} // namespace blaze4k
