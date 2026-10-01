#pragma once

#include "screens/screen.hpp"

namespace blaze4k {

// Attract (title loop) screen: an animated logo/receptor loop. Confirm is handled
// centrally by the ScreenManager (which returns to the origin screen), so this
// screen stays dumb (PRD section 6 pattern 4). Real gameplay autoplay is deferred.
class AttractScreen : public Screen {
public:
    [[nodiscard]] ScreenId id() const override { return ScreenId::Attract; }

    void enter(ScreenContext& ctx) override;
    void update(ScreenContext& ctx, double fixed_dt, const std::vector<InputEvent>& events) override;
    void render(ScreenContext& ctx, GlQuadRenderer& renderer, int w, int h) override;

private:
    double phase_seconds_ = 0.0;
};

} // namespace blaze4k