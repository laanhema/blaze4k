#pragma once

#include <array>
#include <vector>
#include "gameplay/note_field.hpp"
#include "gameplay/noteskin.hpp"
#include "render/gl_quad_renderer.hpp"

namespace blaze4k {

// Most recent tap explosion in a column. `window` Num = none.
struct TapExplosionState {
    TapJudgment window = TapJudgment::Num;
    double elapsed = 0.0;  // music seconds since the explosion started
    double duration = 0.0; // kCelTapExplosionSeconds, or kCelHeldExplosionSeconds
};

// Per-frame presentation inputs for the note field. Cosmetic only: they drive
// skin animation and receptor feedback, never judgment.
struct NoteFieldFrame {
    double beat = 0.0;          // song beat at the current music time
    double music_seconds = 0.0; // current music time
    std::array<float, 4> receptor_zoom{1.0f, 1.0f, 1.0f, 1.0f}; // press bump per column
    std::array<TapExplosionState, 4> tap_explosion{};
    std::array<bool, 4> hold_explosion{false, false, false, false}; // a hold/roll is active
    std::array<double, 4> mine_explosion_elapsed{1e9, 1e9, 1e9, 1e9}; // seconds since a mine hit
};

// Screen-y layout of one hold/roll (pure, no GL). `d` = +1 when the tail is drawn below the
// head (up-scroll), -1 for down-scroll. The body runs head_y..body_end_y; the cap starts at
// body_end_y and extends cap_size further (centred on the tail for Cel). Like OpenITG
// DrawHoldBottomCap (up-scroll, mirrored for reverse), no part of the cap is drawn on the head
// side of head_y.
struct HoldLayout {
    double body_end_y = 0.0;  // body's tail-side edge == the cap's unclipped head-side edge
    bool has_body = false;    // false once the head reaches/passes body_end_y
    bool has_cap = false;     // false without cap art, or when the head is past the whole cap
    double cap_near_y = 0.0;  // cap head-side edge after clipping at head_y
    double cap_far_y = 0.0;   // cap tail-side edge: the visible end of the hold
    float cap_v_near = 0.0f;  // cap texture v at cap_near_y (0 unclipped; art is head-on-top)
    UVRect cap_uv{};          // cap quad UVs, top to bottom on screen (v flipped in reverse)
};
[[nodiscard]] HoldLayout layout_hold(double head_y, double tail_y, bool reverse,
                                     double cap_size, double tail_inset, bool has_cap);

// Turns `NoteField` layout output into draw calls on a `GlQuadRenderer`.
// Draw order: receptor row, hold/roll bodies + end caps (each cap centred on its
// tail for Cel, see `layout_hold`), heads, mines, then explosions on top
// (StepMania draws the GhostArrowRow last).
class NoteFieldRenderer {
public:
    void render(const NoteField& field,
                const std::vector<NoteRenderItem>& items,
                const NoteFieldFrame& frame,
                int screen_w,
                int screen_h,
                const NoteSkin& skin,
                GlQuadRenderer& renderer) const;

private:
    mutable int last_drawn_quads_ = 0;
};

} // namespace blaze4k
