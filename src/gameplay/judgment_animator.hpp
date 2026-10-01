#pragma once

#include <string>
#include <vector>

#include "gameplay/judgment.hpp"
#include "render/geometry.hpp"

namespace blaze4k {

class GlQuadRenderer;

// Presentation-only animator for judgment and combo popups. It is fed the exact
// `new_events_` log slice that `ScoreKeeper`/`LifeKeeper` consume (no re-judging,
// no clock in the judgment path): `consume` arms a popup from the latest visible
// event, `update` advances the fade with `fixed_dt` (presentation only), and
// `render` draws through the shared bitmap font (a no-op when headless).
class JudgmentAnimator {
public:
    // Blaze 4k presentation constants (unsourced; no OpenITG parity requirement).
    static constexpr double kJudgmentPopSeconds = 0.6;
    static constexpr double kComboPopSeconds = 0.5;
    static constexpr int kComboMilestone = 50;

    void reset();

    // Arms a popup from the last score/combo-bearing event in `events` (a chord
    // resolving in one tick: last event wins). AvoidedMine/RollHit do not pop.
    void consume(const std::vector<JudgmentEvent>& events);

    // Advances popup timers and arms a deduped combo pop when `combo` crosses a
    // `kComboMilestone` multiple (chord jumps included). A combo of 0 resets the
    // milestone tracker so rebuilding to the same multiple after a miss re-fires.
    void update(double fixed_dt, int combo);

    // Arms a combo pop for an arbitrary combo (e.g. the final/max combo at chart
    // completion), bypassing the milestone filter. `combo <= 0` is ignored.
    void celebrate(int combo);

    void render(GlQuadRenderer& renderer, int w, int h) const;

    // Pure pop curves, clamped so a pop never reverses or overshoots the label.
    // `pop_scale` eases up to ~1.25 then settles to 1.0; `pop_alpha` holds at 1
    // then fades to 0; `pop_active` is true while `elapsed < duration`.
    [[nodiscard]] static float pop_scale(double elapsed, double duration);
    [[nodiscard]] static float pop_alpha(double elapsed, double duration);
    [[nodiscard]] static bool pop_active(double elapsed, double duration);

    // Pure label/color mapping from an event; an empty label means "no popup".
    [[nodiscard]] static std::string judgment_label(const JudgmentEvent& e);
    [[nodiscard]] static Color judgment_color(const JudgmentEvent& e);

    [[nodiscard]] bool has_popup() const { return has_pop_; }
    [[nodiscard]] const std::string& popup_label() const { return pop_label_; }
    [[nodiscard]] bool has_combo_pop() const { return has_combo_pop_; }
    [[nodiscard]] int combo_value() const { return combo_value_; }

private:
    bool has_pop_ = false;
    std::string pop_label_;
    Color pop_color_{};
    double pop_elapsed_ = 0.0;

    bool has_combo_pop_ = false;
    double combo_elapsed_ = 0.0;
    int combo_value_ = 0;
    int last_milestone_ = 0;
};

} // namespace blaze4k
