#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "audio/metronome.hpp"
#include "data/config.hpp"
#include "data/config_loader.hpp"
#include "gameplay/gameplay_options.hpp"
#include "gameplay/judgment_input.hpp"
#include "render/gl_quad_renderer.hpp"
#include "screens/calibration_screen.hpp"
#include "screens/screen.hpp"
#include "screens/screen_manager.hpp"
#include "timing/music_clock.hpp"

namespace fs = std::filesystem;

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

using td::CalibrationConfig;
using td::GameAction;
using td::InputEvent;
using td::MusicClock;
using td::ScreenContext;
using td::ScreenId;

constexpr uint64_t kRefNs = 1'000'000'000; // 1 s SDL reference
constexpr double kSampleRate = 48000.0;

bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }

// In-memory fake driven through the IAudioStream seam (mirrors
// tests/preview_player_test.cpp). Lets the wizard be exercised headless.
class FakeAudioStream : public td::IAudioStream {
public:
    bool load(const std::string& filepath) override {
        loaded_path = filepath;
        loaded = load_result;
        playing = false;
        return load_result;
    }
    void stop() override { playing = false; }
    bool play() override {
        playing = play_result;
        return play_result;
    }
    bool seek_seconds(double seconds) override {
        position = seconds;
        return true;
    }
    [[nodiscard]] double get_position_seconds() const override { return position; }
    [[nodiscard]] bool is_playing() const override { return playing; }
    void set_volume(float /*volume*/) override {}

    bool load_result = true;
    bool play_result = true;
    bool loaded = false;
    bool playing = false;
    double position = 0.0;
    std::string loaded_path;
};

struct FrameState {
    uint64_t frames = 0;
    uint32_t rate = 48000;
};

class SelectSpy : public td::Screen {
public:
    explicit SelectSpy(int* enters = nullptr) : enters_(enters) {}
    [[nodiscard]] ScreenId id() const override { return ScreenId::Select; }
    void enter(ScreenContext& /*ctx*/) override {
        if (enters_ != nullptr) {
            ++(*enters_);
        }
    }

private:
    int* enters_ = nullptr;
};

InputEvent press(GameAction action, uint64_t ts_ns) {
    InputEvent event;
    event.action = action;
    event.pressed = true;
    event.timestamp_ns = ts_ns;
    return event;
}

void set_frame(FrameState& frame, double music_seconds) {
    frame.frames = static_cast<uint64_t>(std::llround(music_seconds * frame.rate));
}

// Owns the wizard under test plus the manager/config/context it needs. The
// injected fake source is driven from `frame`, so every update samples an exact,
// deterministic music time with no device and no wall clock.
struct Fixture {
    FrameState frame;
    FakeAudioStream stream;
    td::GameConfig config;
    CalibrationConfig ccfg;
    td::ScreenManager manager{0.0};
    td::CalibrationScreen* cal = nullptr;
    int select_enters = 0;

    explicit Fixture(CalibrationConfig config = {}) : ccfg(config) {}

    void start(double initial_offset = 0.0) {
        config.offset.global_offset_seconds = initial_offset;
        auto owner = std::make_unique<td::CalibrationScreen>(
            stream,
            [this] { return td::SamplePosition{frame.frames, frame.rate}; },
            ccfg);
        cal = owner.get();
        manager.add_screen(std::move(owner));
        manager.add_screen(std::make_unique<SelectSpy>(&select_enters));
        manager.context().config = &config;
        manager.context().input_reference_ns = kRefNs;
        manager.start(ScreenId::Calibration);
    }

    // Feeds one panel press whose aged hit time lands `hit_music` on the clock.
    void tap(double reference_music, uint64_t ts_ns, GameAction action = GameAction::Left) {
        set_frame(frame, reference_music);
        manager.update(0.0, {press(action, ts_ns)});
    }
};

// Eight taps, each aged 5 ms from a reference 30 ms after its beat. The aged hit
// therefore sits exactly +30 ms after the beat -> offset -30 ms.
void collect_eight_late_taps(Fixture& fx, double bias = 0.030) {
    const uint64_t age_ns = 5'000'000;
    for (int i = 0; i < 8; ++i) {
        const double reference =
            fx.ccfg.beat_time(i) + bias + static_cast<double>(age_ns) / 1e9;
        fx.tap(reference, kRefNs - age_ns);
    }
}

void test_measure_through_gameplay_path() {
    Fixture fx;
    fx.start();

    const double bias = 0.030;
    const uint64_t age_ns = 5'000'000;
    double expected_sum = 0.0;
    for (int i = 0; i < 8; ++i) {
        const double reference = fx.ccfg.beat_time(i) + bias + static_cast<double>(age_ns) / 1e9;
        const uint64_t ts = kRefNs - age_ns;
        // Called directly: proves the screen went through the exact same helper.
        expected_sum += td::music_time_for_event(ts, kRefNs, reference) - fx.ccfg.beat_time(i);
        fx.tap(reference, ts);
    }

    TEST_CHECK(fx.cal->sample_count() == 8);
    TEST_CHECK(fx.cal->result().ready);
    const double expected_mean = expected_sum / 8.0;
    TEST_CHECK(near(expected_mean, bias, 1e-9));
    TEST_CHECK(near(fx.cal->result().mean_delta_seconds, expected_mean, 1e-9));
    TEST_CHECK(near(fx.cal->result().offset_seconds, -bias, 1e-9));
    std::cout << "  - measured through music_time_for_event path ok.\n";
}

void test_confirm_saves_and_transitions() {
    Fixture fx;
    fx.start();
    collect_eight_late_taps(fx);

    const double offset = fx.cal->result().offset_seconds;
    TEST_CHECK(near(offset, -0.030, 1e-9));

    fx.manager.update(0.0, {press(GameAction::Confirm, kRefNs)});
    TEST_CHECK(fx.cal->saved());
    TEST_CHECK(near(fx.config.offset.global_offset_seconds, offset, 1e-12));
    TEST_CHECK(fx.manager.active_id() == ScreenId::Select);
    TEST_CHECK(fx.select_enters == 1);
    std::cout << "  - Confirm writes config + transitions to Select ok.\n";
}

void test_b1_application() {
    Fixture fx;
    fx.start();
    collect_eight_late_taps(fx);
    fx.manager.update(0.0, {press(GameAction::Confirm, kRefNs)});

    const double offset = fx.config.offset.global_offset_seconds;
    const td::GameplayOptions options = td::gameplay_options_from_config(fx.config);
    TEST_CHECK(near(options.global_offset_seconds, offset, 1e-12));

    MusicClock clock([] { return td::SamplePosition{48000, 48000}; });
    clock.set_global_offset_seconds(options.global_offset_seconds);
    TEST_CHECK(near(clock.sample_time_seconds(), 1.0, 1e-9));
    TEST_CHECK(near(clock.time_seconds(), 1.0 + offset, 1e-9));
    std::cout << "  - B1 gameplay path applies the saved offset ok.\n";
}

void test_persistence_round_trip() {
    Fixture fx;
    fx.start();
    collect_eight_late_taps(fx);
    fx.manager.update(0.0, {press(GameAction::Confirm, kRefNs)});
    const double offset = fx.config.offset.global_offset_seconds;
    TEST_CHECK(near(offset, -0.030, 1e-9));

    const fs::path dir = fs::temp_directory_path() / "td_cal_screen_test_roundtrip";
    fs::remove_all(dir);
    const fs::path config_path = dir / "config.json";
    std::string message;
    TEST_CHECK(td::save_config(config_path, fx.config, &message));

    td::ConfigLoadStatus status = td::ConfigLoadStatus::UsedDefaults;
    const td::GameConfig loaded = td::load_config(config_path, &message, &status);
    TEST_CHECK(status == td::ConfigLoadStatus::LoadedFromFile);
    TEST_CHECK(near(loaded.offset.global_offset_seconds, offset, 1e-12));
    fs::remove_all(dir);
    std::cout << "  - save/load config round-trip preserves the offset ok.\n";
}

void test_abort_retains_previous_offset() {
    Fixture fx;
    fx.start(0.123);
    collect_eight_late_taps(fx, 0.020); // would compute a different offset if saved

    fx.manager.update(0.0, {press(GameAction::Back, kRefNs)});
    TEST_CHECK(fx.manager.active_id() == ScreenId::Select);
    TEST_CHECK(near(fx.config.offset.global_offset_seconds, 0.123, 1e-12));
    TEST_CHECK(!fx.cal->saved());
    std::cout << "  - Back abort retains the previous offset ok.\n";
}

void test_synthetic_refuses_to_save() {
    FakeAudioStream failing;
    failing.load_result = false;

    td::GameConfig config;
    config.offset.global_offset_seconds = 0.05;

    auto owner = std::make_unique<td::CalibrationScreen>(
        failing, MusicClock::Source{}, CalibrationConfig{});
    td::CalibrationScreen* cal = owner.get();
    td::ScreenManager manager(0.0);
    manager.add_screen(std::move(owner));
    manager.add_screen(std::make_unique<SelectSpy>());
    manager.context().config = &config;
    manager.context().input_reference_ns = kRefNs;
    manager.start(ScreenId::Calibration);

    TEST_CHECK(!cal->audio_available());

    // Synthetic clock advances only from fixed_dt; 0.5 s steps land on beats.
    for (int i = 0; i < 4; ++i) {
        manager.update(0.5, {});
    }
    for (int i = 0; i < 8; ++i) {
        manager.update(0.5, {press(GameAction::Left, kRefNs - 30'000'000)});
    }
    TEST_CHECK(cal->sample_count() >= 8);
    TEST_CHECK(cal->result().ready);

    manager.update(0.0, {press(GameAction::Confirm, kRefNs)});
    TEST_CHECK(!cal->saved());
    TEST_CHECK(manager.active_id() == ScreenId::Calibration);
    TEST_CHECK(near(config.offset.global_offset_seconds, 0.05, 1e-12));
    std::cout << "  - synthetic (no audio) refuses to save ok.\n";
}

void test_zero_and_future_timestamps_are_safe() {
    Fixture fx;
    fx.start();

    const double reference = fx.ccfg.beat_time(0) + 0.030;
    set_frame(fx.frame, reference);
    fx.manager.update(0.0, {press(GameAction::Left, 0)});             // unset timestamp
    fx.manager.update(0.0, {press(GameAction::Right, kRefNs + 1000)}); // future timestamp

    TEST_CHECK(fx.cal->sample_count() == 2);
    TEST_CHECK(std::isfinite(fx.cal->result().mean_delta_seconds));
    TEST_CHECK(near(fx.cal->result().mean_delta_seconds, 0.030, 1e-9));
    std::cout << "  - zero/future event timestamps are safe ok.\n";
}

void test_render_and_reenter_reset() {
    Fixture fx;
    fx.start();
    collect_eight_late_taps(fx);
    TEST_CHECK(fx.cal->sample_count() == 8);

    td::GlQuadRenderer renderer; // uninitialized: draws are no-ops
    fx.manager.render(renderer, 1280, 720);

    fx.cal->exit(fx.manager.context());
    fx.cal->enter(fx.manager.context());
    TEST_CHECK(fx.cal->sample_count() == 0);
    TEST_CHECK(fx.cal->phase() == td::CalibrationPhase::CountIn);
    TEST_CHECK(!fx.cal->saved());
    std::cout << "  - headless render + re-enter reset ok.\n";
}

void test_click_track_and_metronome() {
    const fs::path dir = fs::temp_directory_path() / "td_cal_screen_test_click";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path wav = dir / "click.wav";

    td::MetronomeConfig mc;
    mc.beats = 4;
    mc.lead_in_seconds = 0.5;
    TEST_CHECK(td::write_click_track(wav, mc));
    TEST_CHECK(fs::exists(wav));
    TEST_CHECK(fs::file_size(wav) > 44);

    {
        std::ifstream in(wav, std::ios::binary);
        char tag[4] = {};
        in.read(tag, 4);
        TEST_CHECK(std::string(tag, 4) == "RIFF");
        in.seekg(8);
        in.read(tag, 4);
        TEST_CHECK(std::string(tag, 4) == "WAVE");
    }

    FakeAudioStream stream;
    td::Metronome metronome(stream);
    TEST_CHECK(!metronome.prepare({}, mc)); // empty path -> stub, no device
    TEST_CHECK(metronome.using_stub());

    TEST_CHECK(metronome.prepare(wav, mc));
    TEST_CHECK(!metronome.using_stub());
    metronome.start();
    TEST_CHECK(metronome.is_playing());

    stream.position = 1.0;
    MusicClock clock(metronome.clock_source());
    TEST_CHECK(near(clock.sample_time_seconds(), 1.0, 1e-6));

    metronome.stop();
    TEST_CHECK(!metronome.is_playing());
    fs::remove_all(dir);
    std::cout << "  - click-track synthesis + metronome clock source ok.\n";
}

} // namespace

int main() {
    std::cout << "[calibration_screen_test] Running CalibrationScreen tests...\n";
    test_measure_through_gameplay_path();
    test_confirm_saves_and_transitions();
    test_b1_application();
    test_persistence_round_trip();
    test_abort_retains_previous_offset();
    test_synthetic_refuses_to_save();
    test_zero_and_future_timestamps_are_safe();
    test_render_and_reenter_reset();
    test_click_track_and_metronome();
    std::cout << "[calibration_screen_test] All tests passed!\n";
    return 0;
}