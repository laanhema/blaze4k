#pragma once

#include <memory>
#include <vector>

#include "screens/screen.hpp"

namespace td {

// Owns every registered screen and applies explicit transitions at frame
// boundaries (exit of the outgoing screen, then enter of the incoming one).
// Also implements the central idle-attract policy: when the active screen is
// Title or Select and no input arrives for `idle_timeout_seconds`, it switches
// to Attract, remembering the origin so a Confirm press returns there.
//
// Back-navigation contract: Back from Attract -> its origin, Select -> Title,
// Gameplay -> Select (abort), Results -> Select, Calibration -> Select (abort;
// never writes the offset), and InputRemap -> Select. Only Title does not
// consume Back (the App handles Escape-quit there). Before any of this, the
// active screen's handle_back() hook is consulted; if it returns true (e.g.
// C4's options overlay closing), the default navigation is skipped.
class ScreenManager {
public:
    explicit ScreenManager(double idle_timeout_seconds = 30.0);

    void add_screen(std::unique_ptr<Screen> screen); // keyed by screen->id()
    void start(ScreenId initial);                    // calls enter() once
    void transition_to(ScreenId target);             // deferred request

    void update(double fixed_dt, const std::vector<InputEvent>& events);
    void render(GlQuadRenderer& renderer, int screen_w, int screen_h);

    [[nodiscard]] ScreenId active_id() const { return active_id_; }
    [[nodiscard]] Screen* active_screen() const;
    [[nodiscard]] bool has_screen(ScreenId id) const;
    // True wherever Back is consumed by the shell: the default navigation set
    // below, or the active screen's in-screen modal state (back_consumed()).
    [[nodiscard]] bool back_navigates() const;
    [[nodiscard]] ScreenId attract_return() const { return attract_return_; }

    // Shared state attached by main (config/scores) before the first update.
    [[nodiscard]] ScreenContext& context() { return ctx_; }

    void set_idle_timeout_seconds(double seconds); // <=0 disables idle->Attract
    [[nodiscard]] double idle_timeout_seconds() const { return idle_timeout_seconds_; }
    [[nodiscard]] double idle_seconds() const { return idle_seconds_; }

private:
    void apply_pending(); // exit() old then enter() new; logs each transition
    void handle_back();

    ScreenContext ctx_;
    std::vector<std::unique_ptr<Screen>> screens_;
    ScreenId active_id_ = ScreenId::Title;
    ScreenId pending_id_ = ScreenId::Title;
    bool has_pending_ = false;
    bool started_ = false;
    ScreenId attract_return_ = ScreenId::Title;
    double idle_timeout_seconds_ = 30.0;
    double idle_seconds_ = 0.0;
};

} // namespace td