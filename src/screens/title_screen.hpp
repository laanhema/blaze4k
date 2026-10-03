#pragma once

#include "screens/screen.hpp"

namespace blaze4k {

// Title screen (Cabinet v3, #92): bg_title, logo, subtitle, four Cel tap notes,
// blinking PRESS START plate, footer, scanlines. Confirm advances toward Select.
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