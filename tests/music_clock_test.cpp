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
#include "chart/chart.hpp"
#include "gameplay/judgment_engine.hpp"
#include "gameplay/judgment_input.hpp"
#include "timing/judgment_constants.hpp"
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

    // 7a. #81: timed_time_seconds samples the source ONCE and returns the
    //     offset-applied time together with that sample's timestamp_ns.
    {
        int calls = 0;
        blaze4k::MusicClock timed_clock([&calls] {
            ++calls;
            return blaze4k::SamplePosition{48000 + static_cast<uint64_t>(calls) * 480, 48000,
                                           7'000'000'000ULL + static_cast<uint64_t>(calls)};
        });
        timed_clock.set_global_offset_seconds(0.25);
        const blaze4k::TimedMusicTime t = timed_clock.timed_time_seconds();
        TEST_CHECK(calls == 1);
        TEST_CHECK(std::abs(t.seconds - (1.0 + 0.01 + 0.25)) < 1e-12);
        TEST_CHECK(t.timestamp_ns == 7'000'000'001ULL);

        // A legacy 2-field SamplePosition has no timestamp.
        const blaze4k::SamplePosition legacy{48000, 48000};
        TEST_CHECK(legacy.timestamp_ns == 0);
        blaze4k::MusicClock legacy_clock([] { return blaze4k::SamplePosition{96000, 48000}; });
        const blaze4k::TimedMusicTime lt = legacy_clock.timed_time_seconds();
        TEST_CHECK(lt.timestamp_ns == 0);
        TEST_CHECK(std::abs(lt.seconds - 2.0) < 1e-12);
        // No source: zero pair.
        blaze4k::MusicClock empty_clock;
        TEST_CHECK(empty_clock.timed_time_seconds().timestamp_ns == 0);

        // Consistent-pair aging: an event 3 ms before the clock's own timestamp
        // maps to m - 0.003, regardless of the (later) fallback reference.
        const uint64_t fallback = t.timestamp_ns + 9'000'000ULL;
        const uint64_t ref = blaze4k::aging_reference_ns(t.timestamp_ns, fallback);
        TEST_CHECK(ref == t.timestamp_ns);
        TEST_CHECK(std::abs(blaze4k::music_time_for_event(t.timestamp_ns - 3'000'000ULL, ref,
                                                          t.seconds) -
                            (t.seconds - 0.003)) < 1e-12);
        // Without a clock timestamp the fallback reference is used (today's path).
        TEST_CHECK(blaze4k::aging_reference_ns(lt.timestamp_ns, fallback) == fallback);
        std::cout << "  - Timed (seconds, timestamp_ns) pair from one source sample; aging uses it.\n";
    }

    // 7b. Global offset is applied exactly once, with OpenITG's sign, through the
    //     real chain GameplayView uses (gameplay_view.cpp:70,135,153-154,187):
    //     MusicClock -> music_time_for_event -> JudgmentEngine. OpenITG applies
    //     GlobalOffsetSeconds once in the time<->beat conversion (TimingData.cpp:192,255)
    //     and Blaze delta_ms equals OpenITG fTapNoteOffset (Player.cpp:1099).
    {
        const blaze4k::JudgmentConstants& k = blaze4k::JudgmentConstants::compiled_defaults();
        blaze4k::Chart chart;
        blaze4k::Note note;
        note.column = 0;
        note.beat = 4.0;
        note.time_seconds = 2.0; // chart time: song #OFFSET only, no global offset
        note.type = blaze4k::NoteType::Tap;
        chart.notes.push_back(note);

        // Raw stream position 2.0 - 0.050 s: 93600 frames at 48 kHz.
        const uint64_t frames_at = 93600;
        blaze4k::MusicClock chain_clock([frames_at] {
            return blaze4k::SamplePosition{frames_at, 48000};
        });
        const uint64_t ref_ns = 10'000'000'000ULL;

        // Offset +0.050: music time lands exactly on the note.
        chain_clock.set_global_offset_seconds(0.050);
        const double reference_music = chain_clock.time_seconds();
        TEST_CHECK(std::abs(reference_music - 2.0) < 1e-12);
        {
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(0, blaze4k::music_time_for_event(ref_ns, ref_ns, reference_music));
            TEST_CHECK(engine.events().size() == 1);
            const blaze4k::JudgmentEvent& e = engine.events().front();
            TEST_CHECK(e.kind == blaze4k::JudgmentKind::Tap);
            TEST_CHECK(std::abs(e.delta_ms) < 1e-6);
            TEST_CHECK(e.window == blaze4k::TapJudgment::Fantastic);
        }

        // Input path adds no second offset: an event aged 5 ms is exactly 5 ms
        // earlier than the clock sample it is aged against.
        TEST_CHECK(blaze4k::music_time_for_event(ref_ns - 5'000'000ULL, ref_ns, reference_music) ==
                   reference_music - 0.005);

        // Offset +0.010 with the same frames: music time 1.96, hit 40 ms early,
        // so delta_ms is negative (OpenITG fTapNoteOffset = -fNoteOffset).
        chain_clock.set_global_offset_seconds(0.010);
        {
            blaze4k::JudgmentEngine engine;
            engine.reset(&chart, &k);
            engine.handle_step(0, blaze4k::music_time_for_event(ref_ns, ref_ns,
                                                                chain_clock.time_seconds()));
            TEST_CHECK(engine.events().size() == 1);
            const blaze4k::JudgmentEvent& e = engine.events().front();
            TEST_CHECK(std::abs(e.delta_ms - (-40.0)) < 1e-6);
            TEST_CHECK(e.window == blaze4k::TapJudgment::Excellent);
        }
        std::cout << "  - Global offset applied once through clock -> input -> engine, OpenITG sign.\n";
    }

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
