#include "gameplay/noteskin.hpp"

#include <array>
#include <iostream>
#include <vector>

#include <glad/glad.h>

#include "render/note_art.hpp"

namespace td {

namespace {

// Mask resolution: note heads draw at 56px in a 720p frame, so 64px keeps the
// procedural edges crisp without a meaningful memory cost.
constexpr int kMaskSize = 64;

const std::array<ArrowDirection, 4>& directions() {
    static const std::array<ArrowDirection, 4> kDirections = {
        ArrowDirection::Left,
        ArrowDirection::Down,
        ArrowDirection::Up,
        ArrowDirection::Right,
    };
    return kDirections;
}

Texture upload_mask(const std::vector<uint8_t>& bytes) {
    return Texture::from_rgba(kMaskSize, kMaskSize, bytes.data());
}

} // namespace

NoteSkin::NoteSkin() {
    // Tap/hold/roll heads are neutral white masks: the quantization color (ITG
    // palette) supplies their hue at draw time, so the note's beat subdivision —
    // not its column — drives the color and reads as a timing aid. Type still
    // reads through shape (hold shoulder, roll dashes) and mine keeps its own
    // red art.
    const Color white{1.0f, 1.0f, 1.0f, 1.0f};
    tap_style_ = NoteStyle{white, white, white, 56.0, 56.0};
    hold_style_ = NoteStyle{white, white, white, 56.0, 56.0};
    roll_style_ = NoteStyle{white, white, white, 56.0, 56.0};

    const Color red{1.00f, 0.25f, 0.25f, 1.0f};
    mine_style_ = NoteStyle{red, red, red, 40.0, 40.0};
}

bool NoteSkin::init() {
    if (white_.valid() && receptor_[2].valid()) {
        return true;
    }

    if (glad_glGenTextures == nullptr) {
        std::cerr << "[NoteSkin] No GL context available; noteskin disabled\n";
        return false;
    }

    white_ = Texture::solid(Color{1.0f, 1.0f, 1.0f, 1.0f});
    for (std::size_t column = 0; column < directions().size(); ++column) {
        const ArrowDirection dir = directions()[column];
        tap_[column] = upload_mask(make_arrow_rgba(kMaskSize, dir));
        hold_[column] = upload_mask(make_hold_head_rgba(kMaskSize, dir));
        roll_[column] = upload_mask(make_roll_head_rgba(kMaskSize, dir));
        receptor_[column] = upload_mask(make_receptor_rgba(kMaskSize, dir));
    }
    mine_ = upload_mask(make_mine_rgba(kMaskSize));
    body_ = upload_mask(make_body_rgba(kMaskSize));

    if (!white_.valid() || !receptor_[2].valid()) {
        std::cerr << "[NoteSkin] Failed to create noteskin textures (no GL context?)\n";
        return false;
    }
    return true;
}

void NoteSkin::shutdown() {
    white_.destroy();
    for (Texture& texture : tap_) {
        texture.destroy();
    }
    for (Texture& texture : hold_) {
        texture.destroy();
    }
    for (Texture& texture : roll_) {
        texture.destroy();
    }
    for (Texture& texture : receptor_) {
        texture.destroy();
    }
    mine_.destroy();
    body_.destroy();
}

const NoteStyle& NoteSkin::style_for(NoteType type) const {
    switch (type) {
        case NoteType::Tap: return tap_style_;
        case NoteType::HoldHead: return hold_style_;
        case NoteType::RollHead: return roll_style_;
        case NoteType::Mine: return mine_style_;
    }
    return tap_style_;
}

Color NoteSkin::column_tint(int column) const {
    static const std::array<Color, 4> kColumnTints = {
        Color{1.00f, 1.00f, 1.00f, 1.0f},
        Color{0.85f, 0.95f, 1.00f, 1.0f},
        Color{1.00f, 0.90f, 0.95f, 1.0f},
        Color{0.95f, 1.00f, 0.90f, 1.0f},
    };

    if (column < 0 || column >= static_cast<int>(kColumnTints.size())) {
        return Color{1.0f, 1.0f, 1.0f, 1.0f};
    }
    return kColumnTints[static_cast<std::size_t>(column)];
}

Color NoteSkin::quantization_color(NoteQuantization quantization) const {
    // In The Groove note colors, sampled from OpenITG's default noteskin
    // (`NoteSkins/dance/default/_down tap note 8x8.png`, the 8 color rows indexed
    // by NoteType). Hue encodes the note's beat subdivision, so a glance reveals
    // the timing; the arrow shape still carries the column direction.
    switch (quantization) {
        case NoteQuantization::Fourth:          return Color{0.98f, 0.51f, 0.36f, 1.0f}; // orange-red
        case NoteQuantization::Eighth:          return Color{0.36f, 0.67f, 0.98f, 1.0f}; // blue
        case NoteQuantization::Twelfth:         return Color{0.58f, 0.98f, 0.36f, 1.0f}; // green
        case NoteQuantization::Sixteenth:       return Color{0.98f, 0.93f, 0.36f, 1.0f}; // yellow
        case NoteQuantization::TwentyFourth:    return Color{0.67f, 0.36f, 0.98f, 1.0f}; // purple
        case NoteQuantization::ThirtySecond:    return Color{0.36f, 0.98f, 0.82f, 1.0f}; // cyan
        case NoteQuantization::FortyEighth:     return Color{0.89f, 0.36f, 0.98f, 1.0f}; // pink
        case NoteQuantization::SixtyFourth:     return Color{0.60f, 0.60f, 0.60f, 1.0f}; // grey
        case NoteQuantization::OneNinetySecond: return Color{0.35f, 0.35f, 0.35f, 1.0f}; // dark grey
    }
    return Color{1.0f, 1.0f, 1.0f, 1.0f};
}

const Texture& NoteSkin::select(const std::array<Texture, 4>& textures, int column) {
    if (column < 0 || column >= static_cast<int>(textures.size())) {
        return textures[2]; // Up is the canonical direction.
    }
    return textures[static_cast<std::size_t>(column)];
}

const Texture& NoteSkin::head_texture(NoteType type, int column) const {
    switch (type) {
        case NoteType::Tap: return select(tap_, column);
        case NoteType::HoldHead: return select(hold_, column);
        case NoteType::RollHead: return select(roll_, column);
        case NoteType::Mine: return mine_;
    }
    return select(tap_, column);
}

const Texture& NoteSkin::receptor_texture(int column) const {
    return select(receptor_, column);
}

} // namespace td
