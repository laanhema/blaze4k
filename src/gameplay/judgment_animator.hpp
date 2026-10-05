#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "gameplay/judgment.hpp"
#include "render/geometry.hpp"
#include "render/theme_layout.hpp"

namespace blaze4k {

class GlQuadRenderer;
class TextRenderer;
class ThemeTextures;

// Presentation-only animator for the Cabinet judgment pop and the persistent combo
// line (#93). It is fed the exact `new_events_` log slice that `ScoreKeeper`/`LifeKeeper`
// consume (no re-judging, no clock in the judgment path): `consume` arms a pop from the
// latest visible event, `update` advances the fade and records the live combo with
// `fixed_dt` (presentation only), and the render calls draw the baked `judgment_<kind>`
// sprite (bitmap label fallback) and the "N COMBO" line under it. Rendering is a no-op on
// an uninitialised renderer (headless). Nothing here reads the music clock.
class JudgmentAnimator {
public:
    // Blaze 4k presentation constants (unsourced; no OpenITG parity requirement).
    static constexpr double kJudgmentPopSeconds = 0.6;
    static constexpr double kComboPopSeconds = 0.5;
    static constexpr int kComboMilestone = 50;
    // Bitmap-font pixel of the fallback label at full size (no theme / missing manifest
    // entry). Drawn at `kJudgmentPopPixel * judgment_draw_scale(...)`.
    static constexpr float kJudgmentPopPixel = 5.0f;
    // Display size of the judgment pop relative to the sprite's native 444x66 content box
    // (and of the fallback label relative to `kJudgmentPopPixel`). Owner estimate on #111
    // ("perhaps 50%"): tune here after a playtest.
    static constexpr float kJudgmentDisplayScale = 0.5f;

    // The combo line shows from this live combo on: OpenITG ShowComboAt=4
    // (openitg @ f2c129f, assets/patch-data/Themes/default/metrics.ini:2985 [Combo];
    // src/Combo.cpp SetCombo hides the number and label while iNum < SHOW_COMBO_AT).
    static constexpr int kShowComboAt = 4;
    // Pen gap between the combo number and "COMBO", reference px (measured from
    // docs/cabinet-theme/reference/cabinet-v3-gameplay.png).
    static constexpr float kComboGap = 6.0f;
    // judgment_* sprite content box in reference px (manifest content 888x132 @2x).
    static constexpr Vec2 kJudgmentContentRef{444.0f, 66.0f};

    // Top-left pen positions (line-box tops) of the combo number and its label.
    struct ComboLineLayout {
        Vec2 number{};
        Vec2 label{};
    };

    void reset();

    // Arms a popup from the last score/combo-bearing event in `events` (a chord
    // resolving in one tick: last event wins). AvoidedMine/RollHit do not pop.
    void consume(const std::vector<JudgmentEvent>& events);

    // Advances popup timers, records the live combo for the combo line, and arms a
    // deduped combo flash when `combo` crosses a `kComboMilestone` multiple (chord
    // jumps included). A combo of 0 resets the milestone tracker so rebuilding to the
    // same multiple after a miss re-fires.
    void update(double fixed_dt, int combo);

    // Arms a combo flash for an arbitrary combo (e.g. the final/max combo at chart
    // completion), bypassing the milestone filter. `combo <= 0` is ignored.
    void celebrate(int combo);

    // The judgment sprite centred on the field (centred on the full-size content box whose
    // top is kJudgmentTop, drawn at `judgment_draw_scale`), scaled and faded by the pop
    // curves. With a null `theme`, or a sprite missing from the manifest,
    // draws the bitmap label instead so feedback never disappears.
    void render_judgment(GlQuadRenderer& renderer, int w, int h, const ThemeTextures* theme) const;
    // The persistent "N COMBO" line at kComboTop (kComboNumber + kComboLabel, group-sheared
    // by kComboGroupShear), while `combo_visible()`. No-op for a null `text`.
    void render_combo(GlQuadRenderer& renderer, int w, int h, TextRenderer* text) const;

    // Pure pop curves, clamped so a pop never reverses or overshoots the label.
    // `pop_scale` eases up to ~1.25 then settles to 1.0; `pop_alpha` holds at 1
    // then fades to 0; `pop_active` is true while `elapsed < duration`.
    [[nodiscard]] static float pop_scale(double elapsed, double duration);
    [[nodiscard]] static float pop_alpha(double elapsed, double duration);
    [[nodiscard]] static bool pop_active(double elapsed, double duration);
    // The judgment pop's draw scale: `kJudgmentDisplayScale * pop_scale(elapsed,
    // kJudgmentPopSeconds)`. The same curve shape, at the display size. Pure.
    [[nodiscard]] static float judgment_draw_scale(double elapsed);

    // Pure label mapping from an event; an empty label means "no popup".
    [[nodiscard]] static std::string judgment_label(const JudgmentEvent& e);
    // Pure theme sprite mapping (judgment_fantastic ... judgment_ng); empty exactly
    // when `judgment_label` is empty. Points at string literals.
    [[nodiscard]] static std::string_view judgment_sprite(const JudgmentEvent& e);

    // Screen rect of the judgment sprite: `content` (its full-size content size at L.s)
    // scaled by `scale` (relative to that full-size `content`) about the centre
    // (L.x(640), L.y(kJudgmentTop) + content.y / 2). Pure.
    [[nodiscard]] static Rect judgment_pop_rect(const theme::LayoutScale& L, Vec2 content,
                                                float scale);
    // Combo line pens: the number + gap + label pen box centred on L.x(640), the number
    // line box top at L.y(kComboTop), the label on the number's baseline. Pure.
    [[nodiscard]] static ComboLineLayout combo_line_layout(const theme::LayoutScale& L,
                                                           float number_w, float label_w,
                                                           float number_ascent,
                                                           float label_ascent);
    // Milestone flash colour of the combo number: kGold at 0, lerping linearly to kWhite
    // at `duration` (and after). Pure.
    [[nodiscard]] static Color combo_number_color(double elapsed, double duration);

    [[nodiscard]] bool has_popup() const { return has_pop_; }
    [[nodiscard]] const std::string& popup_label() const { return pop_label_; }
    [[nodiscard]] std::string_view popup_sprite() const { return pop_sprite_; }
    [[nodiscard]] bool has_combo_pop() const { return has_combo_pop_; }
    [[nodiscard]] int combo_value() const { return combo_value_; }
    [[nodiscard]] int live_combo() const { return live_combo_; }
    [[nodiscard]] bool combo_visible() const { return live_combo_ >= kShowComboAt; }

private:
    bool has_pop_ = false;
    std::string pop_label_;
    std::string_view pop_sprite_;
    double pop_elapsed_ = 0.0;

    bool has_combo_pop_ = false;
    double combo_elapsed_ = 0.0;
    int combo_value_ = 0;
    int last_milestone_ = 0;
    int live_combo_ = 0;
};

} // namespace blaze4k
