#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "gameplay/gameplay_view.hpp"
#include "screens/screen.hpp"

namespace td {

// Deliberately thin host around the already-tested `GameplayView`: it consumes the
// `PlayRequest` published by Select, forwards input/update/render, and returns to
// Select when the run ends. Results/pause/retry belong to C7.
class GameplayScreen : public Screen {
public:
    [[nodiscard]] ScreenId id() const override { return ScreenId::Gameplay; }

    void enter(ScreenContext& ctx) override;
    void update(ScreenContext& ctx, double fixed_dt, const std::vector<InputEvent>& events) override;
    void render(ScreenContext& ctx, GlQuadRenderer& renderer, int w, int h) override;
    void exit(ScreenContext& ctx) override;

    [[nodiscard]] bool is_ready() const { return active_; }

    // Test accessor: the held-state passed to GameplayView for a column.
    [[nodiscard]] bool held(std::size_t column) const { return held_[column]; }

private:
    GameplayView view_;
    std::array<bool, 4> held_{false, false, false, false};
    bool active_ = false;
};

} // namespace td
