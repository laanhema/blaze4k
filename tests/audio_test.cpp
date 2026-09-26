#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <filesystem>
#include "audio/audio_engine.hpp"
#include "audio/sound_stream.hpp"

namespace fs = std::filesystem;

// Helper to write a valid 16-bit mono 44.1kHz PCM WAV file
void write_test_wav(const std::string& path, double duration_sec, double freq_hz = 440.0) {
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

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr << "\n"; \
            std::abort(); \
        } \
    } while (0)

int main() {
    std::cout << "[audio_test] Starting audio engine and sound stream tests...\n";

    // 1. AudioEngine instance test
    td::AudioEngine& engine = td::AudioEngine::instance();
    bool engine_ok = engine.init();
    std::cout << "  - AudioEngine init result: " << (engine_ok ? "SUCCESS" : "FALLBACK") << "\n";
    engine.set_master_volume(0.9f);
    TEST_CHECK(std::abs(engine.get_master_volume() - 0.9f) < 1e-4);

    // 2. Missing file test
    td::SoundStream stream;
    bool load_missing = stream.load("non_existent_file_123456.wav");
    TEST_CHECK(!load_missing);
    TEST_CHECK(!stream.is_loaded());
    std::cout << "  - Missing file handled gracefully (load returned false).\n";

    // 3. Corrupt file test
    fs::path temp_dir = fs::temp_directory_path() / "td_audio_test";
    fs::create_directories(temp_dir);
    fs::path corrupt_path = temp_dir / "corrupt.wav";
    {
        std::ofstream corrupt_out(corrupt_path, std::ios::binary);
        corrupt_out << "NOT A VALID WAV HEADER OR SOUND DATA GARBAGE 123456789";
    }
    bool load_corrupt = stream.load(corrupt_path.string());
    TEST_CHECK(!load_corrupt);
    TEST_CHECK(!stream.is_loaded());
    std::cout << "  - Corrupt file handled gracefully (load returned false).\n";

    // 4. Valid WAV file test
    fs::path valid_path = temp_dir / "test_1s.wav";
    write_test_wav(valid_path.string(), 1.0, 440.0);

    bool load_valid = stream.load(valid_path.string());
    TEST_CHECK(load_valid);
    TEST_CHECK(stream.is_loaded());
    double length = stream.get_length_seconds();
    std::cout << "  - Valid WAV loaded. Length: " << length << "s\n";
    TEST_CHECK(std::abs(length - 1.0) < 0.05);

    // 5. Volume test
    stream.set_volume(0.75f);
    TEST_CHECK(std::abs(stream.get_volume() - 0.75f) < 1e-4);

    // 6. Playback and seeking
    bool play_ok = stream.play();
    TEST_CHECK(play_ok);
    TEST_CHECK(stream.is_playing());

    // Seeking to 0.5s
    bool seek_ok = stream.seek_seconds(0.5);
    TEST_CHECK(seek_ok);
    double pos = stream.get_position_seconds();
    std::cout << "  - Seek to 0.5s reported position: " << pos << "s\n";
    TEST_CHECK(pos >= 0.45 && pos <= 0.6);

    // Pause and Resume
    stream.pause();
    TEST_CHECK(!stream.is_playing());
    stream.resume();
    TEST_CHECK(stream.is_playing());

    // Stop resets position
    stream.stop();
    TEST_CHECK(!stream.is_playing());
    TEST_CHECK(stream.get_position_seconds() == 0.0);

    // Unload
    stream.unload();
    TEST_CHECK(!stream.is_loaded());

    // Cleanup temp files
    fs::remove_all(temp_dir);

    std::cout << "[audio_test] All audio tests passed successfully!\n";
    return 0;
}
