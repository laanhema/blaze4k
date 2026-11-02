#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

#include "input/input_event.hpp"

namespace blaze4k {
class ScreenManager;
class GlQuadRenderer;
class InputManager;
struct GameConfig;
struct HighScores;
class SongLibrary;
struct JudgmentConstants;
struct PlayRequest;
struct ResultsSummary;
class IUiSoundSink;
class ThemeTextures;
class TextRenderer;
class NoteSkin;

enum class ScreenId { Title, Attract, Select, Gameplay, Results, Calibration, InputRemap };

[[nodiscard]] std::string_view screen_id_name(ScreenId id);

struct ScreenContext {
    ScreenManager* manager = nullptr;
    GameConfig* config = nullptr;
    HighScores* scores = nullptr;

    const SongLibrary* library = nullptr;
    const JudgmentConstants* constants = nullptr;
    PlayRequest* play_request = nullptr;
    ResultsSummary* results = nullptr;
    std::uint64_t input_reference_ns = 0;

    // Authoritative per-action down-state, wired by main to
    // InputManager::is_action_down. GameplayScreen samples this for held notes
    // instead of replaying press/release events. Null in headless/unit tests,
    // where callers fall back to event-derived state.
    std::function<bool(GameAction)> action_down;

    std::function<void(bool)> set_cursor_visible;

    // The input layer, wired by main, used by InputRemapScreen to rebuild
    // runtime bindings and toggle raw capture mode. Forward-declared so screens
    // stay free of SDL; null in headless/unit tests.
    InputManager* input = nullptr;

    IUiSoundSink* ui_sounds = nullptr;
    const ThemeTextures* theme = nullptr;
    TextRenderer* text = nullptr;
    const NoteSkin* noteskin = nullptr;
};

class Screen {
public:
    virtual ~Screen() = default;
    [[nodiscard]] virtual ScreenId id() const = 0;
    virtual void enter(ScreenContext& /*ctx*/) {}
    virtual void update(ScreenContext& /*ctx*/, double /*fixed_dt*/,
                        const std::vector<InputEvent>& /*events*/) {}
    virtual void render(ScreenContext& /*ctx*/, GlQuadRenderer& /*renderer*/, int /*w*/,
                        int /*h*/) {}
    virtual void exit(ScreenContext& /*ctx*/) {}
    virtual void update_inactive(double /*fixed_dt*/) {}
    virtual bool handle_back(ScreenContext& /*ctx*/) { return false; }

    // Side-effect-free counterpart to handle_back(): true while an in-screen
    // state (e.g. an open modal) consumes Back. ScreenManager::back_navigates()
    // ORs this with its default navigation set so a modal screen outside that
    // set cannot desync App-quit from handle_back(). Keep the two in sync: any
    // override that makes handle_back() return true must also make this true.
    [[nodiscard]] virtual bool back_consumed() const { return false; }
};

} // namespace blaze4k