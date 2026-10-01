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

// Turns `NoteField` layout output into draw calls on a `GlQuadRenderer`.
// Draw order: receptor row, hold/roll bodies + end caps, heads, mines, then
// explosions on top (StepMania draws the GhostArrowRow last).
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
