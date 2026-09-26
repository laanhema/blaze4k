#pragma once

#include "chart/note.hpp"
#include "render/geometry.hpp"
#include "render/texture.hpp"

namespace td {

struct NoteStyle {
    Color head_color{};
    Color body_color{};
    Color tail_color{};
    double width = 56.0;
    double height = 56.0;
};

// Procedural placeholder noteskin: colored textured quads only, no image files.
// Distinct per note type so taps/holds/rolls/mines read apart until D1/D2.
class NoteSkin {
public:
    NoteSkin();

    bool init(); // requires a GL context; returns false (and logs) if unavailable
    void shutdown();

    [[nodiscard]] const Texture& receptor_texture() const { return receptor_; }
    [[nodiscard]] const Texture& quad_texture() const { return white_; }
    [[nodiscard]] const NoteStyle& style_for(NoteType type) const;
    [[nodiscard]] Color column_tint(int column) const;

private:
    Texture white_;
    Texture receptor_;
    NoteStyle tap_;
    NoteStyle hold_;
    NoteStyle roll_;
    NoteStyle mine_;
};

} // namespace td
