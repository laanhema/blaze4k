#pragma once

#include "screens/screen.hpp"

namespace blaze4k {

// Title screen: logo + blinking "PRESS START". Confirm advances toward Select.
class TitleScreen : public Screen {
public:
    [[nodiscard]] ScreenId id() const override { return ScreenId::Title; }

    void enter(ScreenContext& ctx) override;
    void update(ScreenContext& ctx, double fixed_dt, const std::vector<InputEvent>& events) override;
    void render(ScreenContext& ctx, GlQuadRenderer& renderer, int w, int h) override;

private:
    double blink_seconds_ = 0.0;
};

} // namespace blaze4k