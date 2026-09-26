#pragma once

#include <vector>
#include "gameplay/note_field.hpp"
#include "gameplay/noteskin.hpp"
#include "render/gl_quad_renderer.hpp"

namespace td {

// Turns `NoteField` layout output into draw calls on a `GlQuadRenderer`.
// Draw order: receptor row, hold/roll bodies, tails, heads, mines.
class NoteFieldRenderer {
public:
    void render(const NoteField& field,
                const std::vector<NoteRenderItem>& items,
                int screen_w,
                int screen_h,
                const NoteSkin& skin,
                GlQuadRenderer& renderer) const;

private:
    mutable int last_drawn_quads_ = 0;
};

} // namespace td
