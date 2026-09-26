#include <cstdlib>
#include <iostream>

#include "audio/preview_player.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

using td::PreviewState;
using td::PreviewPlayer;

// In-memory fake driven through the PreviewPlayer stream seam. Lets the
// successful load -> Active -> loop/seek-back path be asserted deterministically
// without an audio device.
class FakeAudioStream : public td::IAudioStream {
public:
    bool load(const std::string& filepath) override {
        ++load_calls;
        loaded_path = filepath;
        loaded = load_result;
        playing = false;
        return load_result;
    }
    void stop() override {
        ++stop_calls;
        playing = false;
        position = 0.0;
    }
    bool play() override {
        ++play_calls;
        playing = true;
        return true;
    }
    bool seek_seconds(double seconds) override {
        ++seek_calls;
        last_seek = seconds;
        position = seconds;
        return true;
    }
    [[nodiscard]] double get_position_seconds() const override { return position; }
    [[nodiscard]] bool is_playing() const override { return playing; }
    void set_volume(float value) override { volume = value; }

    bool load_result = true;
    bool loaded = false;
    bool playing = false;
    double position = 0.0;
    double last_seek = -1.0;
    int load_calls = 0;
    int play_calls = 0;
    int stop_calls = 0;
    int seek_calls = 0;
    float volume = 0.0f;
    std::string loaded_path;
};

void test_successful_load_activates_and_loops() {
    FakeAudioStream stream;
    PreviewPlayer player(stream);
    player.set_delay_seconds(0.5);
    player.set_volume(0.35f);

    player.request("song.ogg", 2.0, 5.0);
    TEST_CHECK(player.state() == PreviewState::Waiting);
    TEST_CHECK(stream.load_calls == 0);

    player.update(0.4);
    TEST_CHECK(player.state() == PreviewState::Waiting);

    player.update(0.2); // crosses the delay
    TEST_CHECK(stream.load_calls == 1);
    TEST_CHECK(stream.loaded_path == "song.ogg");
    TEST_CHECK(stream.play_calls == 1);
    TEST_CHECK(stream.seek_calls == 1);
    TEST_CHECK(stream.last_seek == 2.0); // seeked to the sample start on activation
    TEST_CHECK(stream.volume == 0.35f);
    TEST_CHECK(player.state() == PreviewState::Active);

    // Inside the [start, start+length) window: no seek-back.
    stream.position = 6.9; // window ends at 7.0
    stream.playing = true;
    player.update(0.1);
    TEST_CHECK(stream.seek_calls == 1);

    // Past the window end: wrap back to the start.
    stream.position = 7.0;
    player.update(0.1);
    TEST_CHECK(stream.seek_calls == 2);
    TEST_CHECK(stream.last_seek == 2.0);

    // A paused/stopped stream is not wrapped even if the position runs past.
    stream.playing = false;
    stream.position = 9.0;
    player.update(0.1);
    TEST_CHECK(stream.seek_calls == 2);

    std::cout << "  - successful load -> Active -> loop/seek-back ok.\n";
}

void test_zero_start_loops_from_beginning() {
    FakeAudioStream stream;
    PreviewPlayer player(stream);
    player.set_delay_seconds(0.0);

    player.request("intro.ogg", 0.0, 3.0);
    TEST_CHECK(player.state() == PreviewState::Waiting);
    player.update(0.0); // zero delay loads immediately
    TEST_CHECK(player.state() == PreviewState::Active);
    TEST_CHECK(stream.play_calls == 1);
    TEST_CHECK(stream.seek_calls == 0); // no seek when start is 0

    stream.playing = true;
    stream.position = 3.0;
    player.update(0.1);
    TEST_CHECK(stream.seek_calls == 1);
    TEST_CHECK(stream.last_seek == 0.0);

    std::cout << "  - zero-start preview loops from the beginning ok.\n";
}

void test_failed_load_from_seam_collapses_to_idle() {
    FakeAudioStream stream;
    stream.load_result = false;
    PreviewPlayer player(stream);
    player.set_delay_seconds(0.0);

    player.request("missing.ogg", 1.0, 4.0);
    player.update(0.0);
    TEST_CHECK(stream.load_calls == 1);
    TEST_CHECK(stream.play_calls == 0);
    TEST_CHECK(player.state() == PreviewState::Idle);

    std::cout << "  - seam load failure collapses to Idle ok.\n";
}

void test_delay_scheduling_and_failure() {
    PreviewPlayer player;
    player.set_delay_seconds(0.25);

    player.request("does-not-exist.ogg", 10.0, 12.0);
    TEST_CHECK(player.state() == PreviewState::Waiting);
    TEST_CHECK(player.load_attempts() == 0);
    TEST_CHECK(player.requested_path() == "does-not-exist.ogg");
    TEST_CHECK(player.start_seconds() == 10.0);
    TEST_CHECK(player.length_seconds() == 12.0);

    // Before the delay elapses, no load is attempted.
    player.update(0.1);
    player.update(0.1);
    TEST_CHECK(player.state() == PreviewState::Waiting);
    TEST_CHECK(player.load_attempts() == 0);

    // Crossing the delay triggers exactly one load attempt; a fake path fails
    // gracefully and collapses to Idle.
    player.update(0.1);
    TEST_CHECK(player.load_attempts() == 1);
    TEST_CHECK(player.state() == PreviewState::Idle);

    std::cout << "  - delay scheduling + missing-file failure ok.\n";
}

void test_re_request_resets_timer() {
    PreviewPlayer player;
    player.set_delay_seconds(0.25);
    player.request("first.ogg", 1.0, 2.0);
    player.update(0.3);
    TEST_CHECK(player.load_attempts() == 1);
    TEST_CHECK(player.state() == PreviewState::Idle);

    player.request("second.ogg", 5.0, 8.0);
    TEST_CHECK(player.requested_path() == "second.ogg");
    TEST_CHECK(player.state() == PreviewState::Waiting);
    TEST_CHECK(player.load_attempts() == 1); // unchanged; timer reset
    player.update(0.1);
    TEST_CHECK(player.load_attempts() == 1);
    player.update(0.2);
    TEST_CHECK(player.load_attempts() == 2);

    std::cout << "  - re-request resets the delay timer ok.\n";
}

void test_stop_cancels() {
    PreviewPlayer player;
    player.set_delay_seconds(0.25);
    player.request("track.ogg", 0.0, 5.0);
    player.update(0.1);
    player.stop();
    TEST_CHECK(player.state() == PreviewState::Idle);
    TEST_CHECK(player.requested_path().empty());

    const int attempts_before = player.load_attempts();
    player.update(1.0);
    TEST_CHECK(player.state() == PreviewState::Idle);
    TEST_CHECK(player.load_attempts() == attempts_before); // no reload after stop

    std::cout << "  - stop() cancels the pending preview ok.\n";
}

void test_empty_path_is_idle() {
    PreviewPlayer player;
    player.request("", 3.0, 4.0);
    TEST_CHECK(player.state() == PreviewState::Idle);
    player.update(5.0);
    TEST_CHECK(player.state() == PreviewState::Idle);
    TEST_CHECK(player.load_attempts() == 0);

    player.request("track.ogg", -1.0, -2.0);
    TEST_CHECK(player.state() == PreviewState::Waiting);
    TEST_CHECK(player.start_seconds() == 0.0); // negative clamped to 0
    TEST_CHECK(player.length_seconds() == 0.0);

    std::cout << "  - empty path / non-positive values are safe ok.\n";
}

void test_volume_clamp_no_crash() {
    PreviewPlayer player;
    player.set_volume(2.0f);
    player.set_volume(-1.0f);
    player.set_volume(0.5f);
    TEST_CHECK(player.state() == PreviewState::Idle);

    std::cout << "  - set_volume clamps without crashing ok.\n";
}

} // namespace

int main() {
    std::cout << "[preview_player_test] Running PreviewPlayer tests...\n";
    test_delay_scheduling_and_failure();
    test_re_request_resets_timer();
    test_stop_cancels();
    test_empty_path_is_idle();
    test_volume_clamp_no_crash();
    test_successful_load_activates_and_loops();
    test_zero_start_loops_from_beginning();
    test_failed_load_from_seam_collapses_to_idle();
    std::cout << "[preview_player_test] All tests passed!\n";
    return 0;
}
