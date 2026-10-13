#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "chart/chart.hpp"
#include "gameplay/gameplay_options.hpp"
#include "gameplay/gameplay_view.hpp"
#include "timing/judgment_constants.hpp"

#include "test_wav_writer.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #expr "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

namespace fs = std::filesystem;

constexpr std::array<bool, 4> kNoneHeld{false, false, false, false};

bool approx(double a, double b) {
    return std::abs(a - b) < 1e-9;
}

// One tap at 1.0 s: it is still ahead at chart time 0.25 s and a Miss by 2.0 s.
blaze4k::Chart one_tap_chart() {
    blaze4k::Chart chart;
    blaze4k::Note note;
    note.column = 0;
    note.beat = 1.0;
    note.time_seconds = 1.0;
    note.type = blaze4k::NoteType::Tap;
    chart.notes.push_back(note);
    chart.tap_count = 1;
    return chart;
}

// A 1 s WAV the null audio device can play, so a play can run on the audio clock.
fs::path write_music(const fs::path& dir) {
    fs::create_directories(dir);
    const fs::path wav = dir / "music_1s.wav";
    write_test_wav(wav.string(), 1.0, 440.0);
    return wav;
}

// True once the view's clock advances with no update() call, which only the
// audio clock does: the stub clock stands still without update(). Waits up to 2 s.
bool clock_advances_without_update(const blaze4k::GameplayView& view) {
    for (int i = 0; i < 200; ++i) {
        if (view.music_time_seconds() > 0.01) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

// #138: GameplayScreen reuses one GameplayView, so each load must start the stub
// clock at chart time zero. 0.25 s steps are exact at the stub's 48 kHz.
void test_stub_clock_starts_at_zero_on_each_load() {
    const blaze4k::JudgmentConstants& k = blaze4k::JudgmentConstants::compiled_defaults();
    const blaze4k::Chart chart = one_tap_chart();
    const blaze4k::GameplayOptions options;

    blaze4k::GameplayView view;
    for (int play = 0; play < 2; ++play) {
        TEST_CHECK(view.init(chart, k, "", options));
        TEST_CHECK(approx(view.music_time_seconds(), 0.0));

        view.update(0.25, kNoneHeld);
        TEST_CHECK(approx(view.music_time_seconds(), 0.25));
        TEST_CHECK(view.judgment_events().empty()); // the tap at 1.0 s is still ahead

        for (int i = 0; i < 7; ++i) {
            view.update(0.25, kNoneHeld);
        }
        TEST_CHECK(approx(view.music_time_seconds(), 2.0));
        TEST_CHECK(view.judgment_events().size() == 1); // the tap expired as a Miss
        view.shutdown();
    }
    std::cout << "  - stub clock starts at chart time zero on each load ok.\n";
}

// #138: a play with audio after a play without it runs on the audio clock. The
// audio clock advances with no update() call and cannot pass the end of the 1 s
// file; the stub clock stands still without update() and would read 30 s after
// thirty 1 s steps.
void test_audio_play_after_stub_play_uses_audio_clock(const fs::path& wav) {
    const blaze4k::JudgmentConstants& k = blaze4k::JudgmentConstants::compiled_defaults();
    const blaze4k::Chart chart = one_tap_chart();
    const blaze4k::GameplayOptions options;

    blaze4k::GameplayView view;
    TEST_CHECK(view.init(chart, k, "", options));
    for (int i = 0; i < 4; ++i) {
        view.update(0.25, kNoneHeld);
    }
    TEST_CHECK(approx(view.music_time_seconds(), 1.0));
    view.shutdown();

    TEST_CHECK(view.init(chart, k, wav.string(), options));
    TEST_CHECK(clock_advances_without_update(view));
    for (int i = 0; i < 30; ++i) {
        view.update(1.0, kNoneHeld);
    }
    TEST_CHECK(view.music_time_seconds() <= 1.05);
    view.shutdown();
    std::cout << "  - audio play after a stub play uses the audio clock ok.\n";
}

// #138: a play without audio after a play with it runs on the stub clock, from zero.
// init also returns true when the audio does not load, so the first play is
// checked to be on the audio clock: without that this is "stub after stub".
void test_stub_play_after_audio_play_uses_stub_clock(const fs::path& wav) {
    const blaze4k::JudgmentConstants& k = blaze4k::JudgmentConstants::compiled_defaults();
    const blaze4k::Chart chart = one_tap_chart();
    const blaze4k::GameplayOptions options;

    blaze4k::GameplayView view;
    TEST_CHECK(view.init(chart, k, wav.string(), options));
    TEST_CHECK(clock_advances_without_update(view));
    view.shutdown();

    TEST_CHECK(view.init(chart, k, "", options));
    TEST_CHECK(approx(view.music_time_seconds(), 0.0));
    view.update(0.25, kNoneHeld);
    view.update(0.25, kNoneHeld);
    TEST_CHECK(approx(view.music_time_seconds(), 0.5));
    view.shutdown();
    std::cout << "  - stub play after an audio play uses the stub clock ok.\n";
}

} // namespace

int main() {
    std::cout << "[gameplay_view_test] Running gameplay view tests...\n";
    test_stub_clock_starts_at_zero_on_each_load();

    const fs::path temp_dir = fs::temp_directory_path() / "td_gameplay_view_test";
    const fs::path wav = write_music(temp_dir);
    test_audio_play_after_stub_play_uses_audio_clock(wav);
    test_stub_play_after_audio_play_uses_stub_clock(wav);
    fs::remove_all(temp_dir);

    std::cout << "[gameplay_view_test] All tests passed!\n";
    return 0;
}
