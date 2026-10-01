#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <chrono>
#include <thread>
#include <limits>
#include "audio/audio_engine.hpp"
#include "audio/sound_stream.hpp"
#include "timing/music_clock.hpp"
#include "test_wav_writer.hpp"

namespace fs = std::filesystem;

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr << "\n"; \
            std::abort(); \
        } \
    } while (0)

int main() {
    std::cout << "[music_clock_test] Starting music clock tests...\n";

    // 1. Frame math
    TEST_CHECK(std::abs(blaze4k::MusicClock::seconds_from_pcm(44100, 44100) - 1.0) < 1e-9);
    TEST_CHECK(std::abs(blaze4k::MusicClock::seconds_from_pcm(0, 44100) - 0.0) < 1e-9);
    TEST_CHECK(std::abs(blaze4k::MusicClock::seconds_from_pcm(100, 0) - 0.0) < 1e-9);
    std::cout << "  - Frame math guarded and correct.\n";

    // 2. Offset applied (positive/negative direction)
    blaze4k::MusicClock clock;
    blaze4k::MusicClock::Source static_source = [] {
        return blaze4k::SamplePosition{48000, 48000};
    };
    clock.set_source(static_source);
    TEST_CHECK(clock.has_source());
    clock.set_global_offset_seconds(0.0);
    TEST_CHECK(std::abs(clock.time_seconds() - 1.0) < 1e-9);
    clock.set_global_offset_seconds(0.050);
    TEST_CHECK(std::abs(clock.time_seconds() - 1.050) < 1e-9);
    clock.set_global_offset_seconds(-0.050);
    TEST_CHECK(std::abs(clock.time_seconds() - 0.950) < 1e-9);
    std::cout << "  - Positive/negative offset direction correct.\n";

    // 3. Zero/uncalibrated default
    blaze4k::MusicClock default_clock;
    TEST_CHECK(!default_clock.has_source());
    TEST_CHECK(std::abs(default_clock.time_seconds() - 0.0) < 1e-9);
    TEST_CHECK(std::abs(default_clock.sample_time_seconds() - 0.0) < 1e-9);
    TEST_CHECK(std::abs(default_clock.global_offset_seconds() - 0.0) < 1e-9);
    default_clock.set_source([] { return blaze4k::SamplePosition{0, 44100}; });
    TEST_CHECK(std::abs(default_clock.time_seconds() - 0.0) < 1e-9);
    default_clock.clear_source();
    TEST_CHECK(!default_clock.has_source());
    std::cout << "  - Uncalibrated default yields a functioning clock.\n";

    // 4. Frame-hitch independence: repeated reads between frame changes are identical
    uint64_t frames = 22050;
    blaze4k::MusicClock hitch_clock([&frames] {
        return blaze4k::SamplePosition{frames, 44100};
    });
    double first = hitch_clock.time_seconds();
    for (int i = 0; i < 1000; ++i) {
        TEST_CHECK(hitch_clock.time_seconds() == first);
    }
    std::cout << "  - Clock depends only on provider output (no wall-clock input).\n";

    // 5. Monotonic advance
    frames = 1000;
    double t0 = hitch_clock.time_seconds();
    frames = 2000;
    double t1 = hitch_clock.time_seconds();
    TEST_CHECK(t1 > t0);
    TEST_CHECK(std::abs((t1 - t0) - (1000.0 / 44100.0)) < 1e-12);
    std::cout << "  - Clock advances by exactly the frame delta.\n";

    // 6. Non-finite offset rejection keeps last finite value
    blaze4k::MusicClock offset_clock;
    offset_clock.set_global_offset_seconds(0.050);
    offset_clock.set_global_offset_seconds(std::numeric_limits<double>::quiet_NaN());
    TEST_CHECK(std::abs(offset_clock.global_offset_seconds() - 0.050) < 1e-12);
    offset_clock.set_global_offset_seconds(std::numeric_limits<double>::infinity());
    TEST_CHECK(std::abs(offset_clock.global_offset_seconds() - 0.050) < 1e-12);
    std::cout << "  - Non-finite offsets rejected, last finite value retained.\n";

    // 7. Nanoseconds
    blaze4k::MusicClock ns_clock([] { return blaze4k::SamplePosition{48000, 48000}; });
    ns_clock.set_global_offset_seconds(0.5);
    TEST_CHECK(ns_clock.time_nanoseconds() == 1500000000LL);
    std::cout << "  - Nanosecond conversion correct.\n";

    // 8. Guarded audio integration (End-to-End)
    blaze4k::AudioEngine& engine = blaze4k::AudioEngine::instance();
    if (!engine.init()) {
        std::cout << "[music_clock_test] skipping audio integration (no audio device)\n";
    } else {
        fs::path temp_dir = fs::temp_directory_path() / "td_music_clock_test";
        fs::create_directories(temp_dir);
        fs::path wav_path = temp_dir / "clock_1s.wav";
        write_test_wav(wav_path.string(), 1.0, 440.0);

        blaze4k::SoundStream stream;
        TEST_CHECK(stream.load(wav_path.string()));
        blaze4k::MusicClock audio_clock([&stream] {
            return blaze4k::SamplePosition{stream.get_position_frames(), stream.get_sample_rate()};
        });
        audio_clock.set_global_offset_seconds(0.0);
        TEST_CHECK(audio_clock.time_seconds() >= 0.0);
        TEST_CHECK(stream.play());

        bool advanced = false;
        for (int i = 0; i < 200; ++i) {
            if (audio_clock.time_seconds() > 0.01) {
                advanced = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        TEST_CHECK(advanced);
        std::cout << "  - Audio-bound clock advanced from 0 (t=" << audio_clock.time_seconds() << "s).\n";

        stream.stop();
        stream.unload();
        fs::remove_all(temp_dir);
    }

    std::cout << "[music_clock_test] All music clock tests passed successfully!\n";
    return 0;
}
