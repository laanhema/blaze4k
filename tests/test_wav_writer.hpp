#pragma once

#include <fstream>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <string>

// Helper to write a valid 16-bit mono 44.1kHz PCM WAV file
inline void write_test_wav(const std::string& path, double duration_sec, double freq_hz = 440.0) {
    uint32_t sample_rate = 44100;
    uint16_t num_channels = 1;
    uint16_t bits_per_sample = 16;
    auto num_samples = static_cast<uint32_t>(duration_sec * sample_rate);
    uint32_t data_size = num_samples * num_channels * (bits_per_sample / 8);
    uint32_t chunk_size = 36 + data_size;
    uint32_t byte_rate = sample_rate * num_channels * (bits_per_sample / 8);
    uint16_t block_align = num_channels * (bits_per_sample / 8);

    std::ofstream out(path, std::ios::binary);
    assert(out.is_open());

    // RIFF header
    out.write("RIFF", 4);
    out.write(reinterpret_cast<const char*>(&chunk_size), 4);
    out.write("WAVE", 4);

    // fmt subchunk
    out.write("fmt ", 4);
    uint32_t subchunk1_size = 16;
    uint16_t audio_format = 1; // PCM
    out.write(reinterpret_cast<const char*>(&subchunk1_size), 4);
    out.write(reinterpret_cast<const char*>(&audio_format), 2);
    out.write(reinterpret_cast<const char*>(&num_channels), 2);
    out.write(reinterpret_cast<const char*>(&sample_rate), 4);
    out.write(reinterpret_cast<const char*>(&byte_rate), 4);
    out.write(reinterpret_cast<const char*>(&block_align), 2);
    out.write(reinterpret_cast<const char*>(&bits_per_sample), 2);

    // data subchunk
    out.write("data", 4);
    out.write(reinterpret_cast<const char*>(&data_size), 4);

    const double two_pi = 6.28318530717958647692;
    for (uint32_t i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sample_rate;
        double sample_val = std::sin(two_pi * freq_hz * t);
        auto sample_int16 = static_cast<int16_t>(sample_val * 30000.0);
        out.write(reinterpret_cast<const char*>(&sample_int16), 2);
    }
}
