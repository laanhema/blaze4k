// #81: integration test of the real onProcess -> ClockTap seqlock -> SoundStream
// interpolation path on miniaudio's null backend (silent, real-time paced, no
// audio device needed, so it runs inside the bwrap sandbox). Bounds are loose
// on purpose so a loaded host does not flake it.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <set>
#include <thread>
#include <vector>

#include "audio/audio_engine.hpp"
#include "audio/sound_stream.hpp"
#include "test_wav_writer.hpp"

namespace fs = std::filesystem;

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

uint64_t steady_ns() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                     std::chrono::steady_clock::now().time_since_epoch())
                                     .count());
}

struct Sample {
    uint64_t interp = 0;
    uint64_t raw = 0;
    uint64_t ts = 0;
};

} // namespace

int main() {
    std::cout << "[clock_tap_test] Starting...\n";
    blaze4k::AudioEngine& engine = blaze4k::AudioEngine::instance();
    engine.configure({480, +[]() -> uint64_t { return steady_ns(); }, /*use_null_backend=*/true});
    if (!engine.init()) {
        std::cout << "[clock_tap_test] SKIP: miniaudio null backend could not be initialized\n";
        return 0;
    }
    const uint32_t engine_rate = engine.engine_sample_rate();
    TEST_CHECK(engine_rate > 0);

    const fs::path dir = fs::temp_directory_path() / "td_clock_tap_test";
    fs::create_directories(dir);
    const fs::path wav = dir / "silent_48k.wav";
    TEST_CHECK(write_silent_wav(wav.string(), 3.0, 48000, 2));

    blaze4k::SoundStream stream;
    TEST_CHECK(stream.load(wav.string()));
    stream.set_volume(0.0f);
    TEST_CHECK(!stream.clock_interpolation_enabled());
    TEST_CHECK(stream.enable_clock_interpolation()); // initialized null backend: must succeed
    TEST_CHECK(stream.enable_clock_interpolation()); // idempotent
    TEST_CHECK(stream.clock_interpolation_enabled());
    TEST_CHECK(stream.play());

    // 1. Poll every 1 ms for 1.5 s.
    std::vector<Sample> samples;
    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(1500)) {
        const blaze4k::TimedFrames timed = stream.get_timed_position_frames();
        const uint64_t raw = stream.get_raw_position_frames();
        samples.push_back(Sample{timed.frames, raw, timed.timestamp_ns});
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const blaze4k::AudioEngine::CallbackStats stats = engine.callback_stats();
    std::cout << "  - null device: " << samples.size() << " samples, callback interval min "
              << stats.min_frames << " / max " << stats.max_frames << " over " << stats.callbacks
              << " callbacks @ " << stats.sample_rate << " Hz\n";
    TEST_CHECK(stats.callbacks > 10);
    TEST_CHECK(samples.size() > 100);

    // The raw read after the interpolated one may be newer; compare the
    // interpolated value with the raw value that existed when it was computed,
    // i.e. never below the previous sample's raw and never below its own floor.
    const uint64_t max_period = std::max<uint64_t>(stats.max_frames, 480) * 48000 / engine_rate;
    std::set<uint64_t> distinct_interp;
    std::set<uint64_t> distinct_raw;
    double lead_sum = 0.0;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const Sample& s = samples[i];
        TEST_CHECK(s.ts != 0);
        if (i > 0) {
            TEST_CHECK(s.interp >= samples[i - 1].interp); // never decreases
            TEST_CHECK(s.ts >= samples[i - 1].ts);
            TEST_CHECK(s.interp >= samples[i - 1].raw);    // >= raw (as of the previous read)
        }
        // Lead over raw is bounded by one device period (+1 frame rounding).
        TEST_CHECK(s.interp <= s.raw + max_period + 1);
        distinct_interp.insert(s.interp);
        distinct_raw.insert(s.raw);
        lead_sum += static_cast<double>(s.interp) - static_cast<double>(s.raw);
    }
    const double mean_lead = lead_sum / static_cast<double>(samples.size());
    std::cout << "  - interpolated distinct " << distinct_interp.size() << " vs raw distinct "
              << distinct_raw.size() << ", mean lead " << mean_lead << " frames\n";
    TEST_CHECK(distinct_interp.size() > 3 * distinct_raw.size());
    // Raw is read after the interpolated value, so the mean signed lead can be
    // pulled below zero only if interpolation never leads; require a positive mean.
    TEST_CHECK(mean_lead > 0.0);

    // 2. Seek: the next reading is the target (pre-seek anchors ignored), within loose bounds.
    TEST_CHECK(stream.seek_seconds(0.5));
    const uint64_t after_seek = stream.get_position_frames();
    std::cout << "  - after seek to 0.5 s: " << after_seek << " frames\n";
    TEST_CHECK(after_seek + max_period + 1 >= 24000);
    TEST_CHECK(after_seek <= 24000 + 4800); // +100 ms slack for a preempted test thread
    // Backward seek.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    TEST_CHECK(stream.seek_seconds(0.1));
    const uint64_t after_back = stream.get_position_frames();
    TEST_CHECK(after_back <= 4800 + 4800);
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // let the cursor move again

    // 3. Pause: frozen (raw). Wait past the in-flight callback first.
    stream.pause();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    const uint64_t paused_a = stream.get_position_frames();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const uint64_t paused_b = stream.get_position_frames();
    TEST_CHECK(paused_a == paused_b);
    TEST_CHECK(stream.get_timed_position_frames().frames == paused_b);
    // Resume: no jump of a whole stale period at the first reading.
    stream.resume();
    const uint64_t resumed = stream.get_position_frames();
    TEST_CHECK(resumed >= paused_b);
    TEST_CHECK(resumed <= paused_b + 4800);
    std::cout << "  - pause froze at " << paused_b << ", resume read " << resumed << "\n";

    // 4. Stop: 0.
    stream.stop();
    TEST_CHECK(stream.get_position_frames() == 0);

    // 5. A second stream replaces the tap; unloading the first (inactive) tap is safe.
    blaze4k::SoundStream second;
    TEST_CHECK(second.load(wav.string()));
    second.set_volume(0.0f);
    TEST_CHECK(second.enable_clock_interpolation());
    TEST_CHECK(second.play());
    TEST_CHECK(stream.play());
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    stream.unload(); // inactive tap: detach is a no-op, engine keeps running
    TEST_CHECK(!stream.clock_interpolation_enabled());
    const blaze4k::TimedFrames second_pos = second.get_timed_position_frames();
    TEST_CHECK(second_pos.timestamp_ns != 0);
    TEST_CHECK(second_pos.frames > 0);

    // 6. Detach handshake under load: repeatedly attach and unload the ACTIVE tap
    //    while the audio thread is publishing into it.
    for (int i = 0; i < 50; ++i) {
        blaze4k::SoundStream churn;
        TEST_CHECK(churn.load(wav.string()));
        churn.set_volume(0.0f);
        TEST_CHECK(churn.enable_clock_interpolation());
        TEST_CHECK(churn.play());
        std::this_thread::sleep_for(std::chrono::milliseconds(1 + (i % 5)));
        churn.unload(); // active tap: Dekker handshake before ma_sound_uninit
    }
    // The churn replaced second's tap; re-attaching makes it active again.
    TEST_CHECK(second.enable_clock_interpolation());

    // 6b. Replace the ACTIVE tap during playback and free the replaced sound at
    //     once (no sleep). Crash/hang smoke test only: unload() keeps the
    //     ma_sound storage allocated, so a missing quiescence wait in attach
    //     would not fail here. The guarantee is the seq_cst handshake argument
    //     in AudioEngine::attach_clock_tap / on_process.
    TEST_CHECK(second.play());
    for (int i = 0; i < 50; ++i) {
        blaze4k::SoundStream replaced;
        TEST_CHECK(replaced.load(wav.string()));
        replaced.set_volume(0.0f);
        TEST_CHECK(replaced.enable_clock_interpolation());
        TEST_CHECK(replaced.play());
        std::this_thread::sleep_for(std::chrono::milliseconds(1 + (i % 5)));
        TEST_CHECK(second.enable_clock_interpolation()); // replaces `replaced`'s tap
        replaced.unload(); // now inactive: no detach wait, ma_sound_uninit right away
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    const blaze4k::TimedFrames replacer_pos = second.get_timed_position_frames();
    TEST_CHECK(replacer_pos.timestamp_ns != 0 && replacer_pos.frames > 0);
    second.unload();
    std::cout << "  - tap replacement, 50 active-tap detach and 50 replace-then-free cycles: "
                 "no crash, no hang.\n";

    // 7. Moving a stream keeps its tap (stable address) working.
    {
        blaze4k::SoundStream a;
        TEST_CHECK(a.load(wav.string()));
        a.set_volume(0.0f);
        TEST_CHECK(a.enable_clock_interpolation());
        TEST_CHECK(a.play());
        blaze4k::SoundStream b(std::move(a));
        TEST_CHECK(b.clock_interpolation_enabled());
        TEST_CHECK(!a.clock_interpolation_enabled());
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        const blaze4k::TimedFrames moved = b.get_timed_position_frames();
        TEST_CHECK(moved.timestamp_ns != 0 && moved.frames > 0);
        b.unload();
    }

    // 8. Stats reset request (per-song gameplay log): zeros until the audio
    //    thread applies it, then counting resumes. Something must play: idle
    //    engine updates carry no frames and are not counted.
    blaze4k::SoundStream song;
    TEST_CHECK(song.load(wav.string()));
    song.set_volume(0.0f);
    TEST_CHECK(song.play());
    const blaze4k::AudioEngine::CallbackStats before_reset = engine.callback_stats();
    TEST_CHECK(before_reset.callbacks > 10);
    engine.reset_callback_stats();
    TEST_CHECK(engine.callback_stats().callbacks < before_reset.callbacks);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const blaze4k::AudioEngine::CallbackStats after_reset = engine.callback_stats();
    TEST_CHECK(after_reset.callbacks > 0 && after_reset.callbacks < before_reset.callbacks);
    TEST_CHECK(after_reset.min_frames > 0);
    TEST_CHECK(after_reset.sample_rate == engine_rate);
    song.unload();

    engine.log_callback_stats("clock_tap_test");
    engine.shutdown();
    fs::remove_all(dir);
    std::cout << "[clock_tap_test] All tests passed.\n";
    return 0;
}
