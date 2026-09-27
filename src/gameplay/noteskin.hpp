#pragma once

#include <array>

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

// Procedural noteskin: white-alpha arrow/body masks generated at init via
// `Texture::from_rgba` (render/note_art.*) and tinted at draw time. Direction is
// baked into per-column textures because a `UVRect` cannot express a 90-degree
// rotation (it is axis-aligned only). No image files, no GL context required to
// construct.
class NoteSkin {
public:
    NoteSkin();

    bool init(); // requires a GL context; returns false (and logs) if unavailable
    void shutdown();

    [[nodiscard]] const NoteStyle& style_for(NoteType type) const;
    [[nodiscard]] Color column_tint(int column) const;
    // ITG note color for the beat subdivision a note lands on (OpenITG
    // `NoteDisplay.cpp` denominator colors). Drives tap/hold/roll tint so timing
    // reads at a glance; column direction is carried by the arrow shape instead.
    [[nodiscard]] Color quantization_color(NoteQuantization quantization) const;
    [[nodiscard]] const Texture& quad_texture() const { return white_; }
    // Direction-aware head art (tap/hold/roll arrows; mine is direction-agnostic).
    [[nodiscard]] const Texture& head_texture(NoteType type, int column) const;
    [[nodiscard]] const Texture& receptor_texture(int column) const;
    [[nodiscard]] const Texture& body_texture() const { return body_; }

private:
    [[nodiscard]] static const Texture& select(const std::array<Texture, 4>& textures, int column);

    Texture white_;
    std::array<Texture, 4> tap_;
    std::array<Texture, 4> hold_;
    std::array<Texture, 4> roll_;
    std::array<Texture, 4> receptor_;
    Texture mine_;
    Texture body_;
    NoteStyle tap_style_;
    NoteStyle hold_style_;
    NoteStyle roll_style_;
    NoteStyle mine_style_;
};

} // namespace td
