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
    const Color cyan{0.20f, 0.90f, 1.00f, 1.0f};
    tap_style_ = NoteStyle{cyan, cyan, cyan, 56.0, 56.0};

    const Color green{0.30f, 1.00f, 0.45f, 1.0f};
    hold_style_ = NoteStyle{green, green, green, 56.0, 56.0};

    const Color amber{1.00f, 0.75f, 0.20f, 1.0f};
    const Color purple{0.70f, 0.40f, 1.00f, 1.0f};
    roll_style_ = NoteStyle{amber, amber, purple, 56.0, 56.0};

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
