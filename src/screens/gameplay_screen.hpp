#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "gameplay/gameplay_view.hpp"
#include "screens/screen.hpp"

namespace td {

// Deliberately thin host around the already-tested `GameplayView`: it consumes the
// `PlayRequest` published by Select, forwards input/update/render, and reports a
// finished run to Results (publishing the run snapshot into `ctx.results`). Back
// aborts straight to Select with no result. On a run end it falls back to Select
// when no Results screen is registered, so a run can never strand here.
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

    // Test accessor: true once the finished run has been reported (snapshot
    // published and transition requested), so a run is never reported twice.
    [[nodiscard]] bool end_reported() const { return end_reported_; }

    // Seconds the field lingers after the run ends before the Results transition,
    // so the final note/fail state is readable instead of snapping away.
    static constexpr double kEndDelaySeconds = 2.0;

private:
    GameplayView view_;
    std::array<bool, 4> held_{false, false, false, false};
    bool active_ = false;
    bool end_reported_ = false;
    double end_delay_elapsed_ = 0.0;
};

} // namespace td
