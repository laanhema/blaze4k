#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "audio/ui_sounds.hpp"
#include "input/input_event.hpp"
#include "screens/screen.hpp"
#include "screens/screen_manager.hpp"

#define TEST_CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr \
                      << "\n"; \
            std::abort(); \
        } \
    } while (0)

namespace {

using blaze4k::GameAction;
using blaze4k::InputEvent;
using blaze4k::ScreenId;
using blaze4k::UiSound;

std::uint16_t read_u16(const std::vector<unsigned char>& bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(bytes[offset + 1] << 8);
}

std::uint32_t read_u32(const std::vector<unsigned char>& bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

std::vector<unsigned char> slurp(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(in),
                                      std::istreambuf_iterator<char>());
}

// Verifies a fixed-layout 16-bit mono PCM WAV and returns its data-chunk size.
std::uint32_t check_wav(const std::filesystem::path& path) {
    const std::vector<unsigned char> bytes = slurp(path);
    TEST_CHECK(bytes.size() >= 44);
    TEST_CHECK(std::string(bytes.begin(), bytes.begin() + 4) == "RIFF");
    TEST_CHECK(std::string(bytes.begin() + 8, bytes.begin() + 12) == "WAVE");
    TEST_CHECK(read_u16(bytes, 20) == 1);    // PCM
    TEST_CHECK(read_u16(bytes, 22) == 1);    // mono
    TEST_CHECK(read_u32(bytes, 24) == 44100);
    TEST_CHECK(read_u16(bytes, 34) == 16);   // 16-bit
    const std::uint32_t data_size = read_u32(bytes, 40);
    TEST_CHECK(data_size == bytes.size() - 44);

    bool nonzero = false;
    for (std::size_t i = 44; i < bytes.size(); ++i) {
        if (bytes[i] != 0) {
            nonzero = true;
            break;
        }
    }
    TEST_CHECK(nonzero);
    return data_size;
}

void test_wav_synth() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "blaze4k_ui_sounds_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    TEST_CHECK(blaze4k::write_ui_sound_wav(root / "move.wav", UiSound::Move));
    TEST_CHECK(blaze4k::write_ui_sound_wav(root / "confirm.wav", UiSound::Confirm));
    TEST_CHECK(blaze4k::write_ui_sound_wav(root / "back.wav", UiSound::Back));

    const std::uint32_t move = check_wav(root / "move.wav");
    const std::uint32_t confirm = check_wav(root / "confirm.wav");
    const std::uint32_t back = check_wav(root / "back.wav");
    TEST_CHECK(move != confirm && confirm != back && move != back);

    // Empty path is rejected.
    TEST_CHECK(!blaze4k::write_ui_sound_wav(std::filesystem::path{}, UiSound::Move));

    std::filesystem::remove_all(root);
    std::cout << "  - synthesized distinct 16-bit mono PCM WAVs ok.\n";
}

InputEvent press(GameAction action) {
    InputEvent event;
    event.action = action;
    event.pressed = true;
    return event;
}

class FakeSink : public blaze4k::IUiSoundSink {
public:
    void play(UiSound sound) override { played.push_back(sound); }
    std::vector<UiSound> played;
};

class FakeScreen : public blaze4k::Screen {
public:
    explicit FakeScreen(ScreenId id) : id_(id) {}
    [[nodiscard]] ScreenId id() const override { return id_; }
    [[nodiscard]] bool back_consumed() const override { return modal; }

    bool modal = false; // stands in for Select's options overlay being open

private:
    ScreenId id_;
};

FakeScreen* add_fake(blaze4k::ScreenManager& manager, ScreenId id) {
    auto screen = std::make_unique<FakeScreen>(id);
    FakeScreen* raw = screen.get();
    manager.add_screen(std::move(screen));
    return raw;
}

void test_menu_triggers() {
    FakeSink sink;
    blaze4k::ScreenManager manager(0.0);
    add_fake(manager, ScreenId::Title);
    add_fake(manager, ScreenId::Attract);
    add_fake(manager, ScreenId::Select);
    add_fake(manager, ScreenId::Gameplay);
    add_fake(manager, ScreenId::Results);
    add_fake(manager, ScreenId::Calibration);
    add_fake(manager, ScreenId::InputRemap);
    manager.context().ui_sounds = &sink;

    manager.start(ScreenId::Select);

    sink.played.clear();
    manager.update(0.1, {press(GameAction::Left)});
    TEST_CHECK(sink.played.size() == 1);
    TEST_CHECK(sink.played[0] == UiSound::Move);

    sink.played.clear();
    manager.update(0.1, {press(GameAction::Confirm)});
    TEST_CHECK(sink.played.size() == 1);
    TEST_CHECK(sink.played[0] == UiSound::Confirm);

    sink.played.clear();
    manager.update(0.1, {press(GameAction::Back)});
    TEST_CHECK(sink.played.size() == 1);
    TEST_CHECK(sink.played[0] == UiSound::Back);

    // Title is also a menu (title screen move blip).
    manager.start(ScreenId::Title);
    sink.played.clear();
    manager.update(0.1, {press(GameAction::Right)});
    TEST_CHECK(sink.played.size() == 1 && sink.played[0] == UiSound::Move);
    std::cout << "  - menu Move/Confirm/Back triggers ok.\n";
}

void test_options_toggle_triggers() {
    FakeSink sink;
    blaze4k::ScreenManager manager(0.0);
    add_fake(manager, ScreenId::Title);
    FakeScreen* select = add_fake(manager, ScreenId::Select);
    manager.context().ui_sounds = &sink;
    manager.start(ScreenId::Select);

    // Opening the overlay (not yet modal) plays Confirm.
    sink.played.clear();
    manager.update(0.1, {press(GameAction::Options)});
    TEST_CHECK(sink.played.size() == 1 && sink.played[0] == UiSound::Confirm);

    // Closing it (modal) plays Back.
    select->modal = true;
    sink.played.clear();
    manager.update(0.1, {press(GameAction::Options)});
    TEST_CHECK(sink.played.size() == 1 && sink.played[0] == UiSound::Back);

    // No duplicate when the same sound already fired from another press.
    select->modal = false;
    sink.played.clear();
    manager.update(0.1, {press(GameAction::Options), press(GameAction::Confirm)});
    TEST_CHECK(sink.played.size() == 1 && sink.played[0] == UiSound::Confirm);

    // Options does nothing on Title, so it stays silent there.
    manager.start(ScreenId::Title);
    sink.played.clear();
    manager.update(0.1, {press(GameAction::Options)});
    TEST_CHECK(sink.played.empty());
    std::cout << "  - Select Options open/close triggers ok.\n";
}

void test_non_menu_screens_silent() {
    FakeSink sink;
    blaze4k::ScreenManager manager(0.0);
    add_fake(manager, ScreenId::Title);
    add_fake(manager, ScreenId::Select);
    add_fake(manager, ScreenId::Gameplay);
    add_fake(manager, ScreenId::Calibration);
    add_fake(manager, ScreenId::InputRemap);
    manager.context().ui_sounds = &sink;

    const ScreenId excluded[3] = {ScreenId::Gameplay, ScreenId::Calibration,
                                  ScreenId::InputRemap};
    for (ScreenId id : excluded) {
        manager.start(id);
        sink.played.clear();
        manager.update(0.1, {press(GameAction::Left), press(GameAction::Confirm),
                             press(GameAction::Back)});
        TEST_CHECK(sink.played.empty());
    }

    // Null sink stays a safe no-op.
    blaze4k::ScreenManager bare(0.0);
    add_fake(bare, ScreenId::Select);
    bare.start(ScreenId::Select);
    bare.update(0.1, {press(GameAction::Left), press(GameAction::Confirm), press(GameAction::Back)});
    std::cout << "  - gameplay/calibration/remap silent + null sink ok.\n";
}

void test_unavailable_player_is_silent() {
    blaze4k::UiSoundPlayer player;
    TEST_CHECK(!player.init(std::filesystem::path{}));
    TEST_CHECK(!player.is_ready());
    player.play(UiSound::Move); // must not crash
    std::cout << "  - unavailable player init false + play no-op ok.\n";
}

} // namespace

int main() {
    std::cout << "[ui_sounds_test] Running UI sound synth + trigger tests...\n";
    test_wav_synth();
    test_menu_triggers();
    test_options_toggle_triggers();
    test_non_menu_screens_silent();
    test_unavailable_player_is_silent();
    std::cout << "[ui_sounds_test] All tests passed!\n";
    return 0;
}
