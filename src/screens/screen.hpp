#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "input/input_event.hpp"

namespace td {
class ScreenManager;
class GlQuadRenderer;
struct GameConfig;
struct HighScores;

// Stable identity of every arcade screen (PRD section 6/7.3). C1 implements
// Title/Attract and a Select placeholder; C3/C4/C7 replace/extend the rest.
enum class ScreenId { Title, Attract, Select, Gameplay, Results };

[[nodiscard]] std::string_view screen_id_name(ScreenId id);

// Services a screen may use. Kept small and free of SDL/GL so screens are
// constructible and updatable headless. C2 extends it with the shared config and
// high-score state (owned by main), so C3-C7 consume them through this seam
// rather than a new global.
struct ScreenContext {
    ScreenManager* manager = nullptr;
    GameConfig* config = nullptr; // C3/C4/C5 read & write; owned by main
    HighScores* scores = nullptr; // C3/C7 read; C7 submits
};

// A screen is an object with explicit enter/update/render/exit (PRD section 6
// pattern 4). update() receives input events already polled by the App for this
// tick; render() receives a renderer that is a safe no-op when headless.
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
};

} // namespace td