#pragma once

#include "screens/screen.hpp"

namespace td {

// Minimal placeholder occupying the Song Select slot so Title -> Select is an
// explicit, testable transition today. C3 replaces this file's registration (not
// the manager) with the real song wheel. Back is handled centrally by the manager.
class SelectPlaceholderScreen : public Screen {
public:
    [[nodiscard]] ScreenId id() const override { return ScreenId::Select; }

    void enter(ScreenContext& ctx) override;
    void update(ScreenContext& ctx, double fixed_dt, const std::vector<InputEvent>& events) override;
    void render(ScreenContext& ctx, GlQuadRenderer& renderer, int w, int h) override;
};

} // namespace td