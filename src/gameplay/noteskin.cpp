#include "gameplay/noteskin.hpp"

#include <array>
#include <iostream>

#include <glad/glad.h>

namespace td {

NoteSkin::NoteSkin() {
    const Color cyan{0.20f, 0.90f, 1.00f, 1.0f};
    tap_ = NoteStyle{cyan, cyan, cyan, 56.0, 56.0};

    const Color green{0.30f, 1.00f, 0.45f, 1.0f};
    hold_ = NoteStyle{green, green, green, 56.0, 56.0};

    const Color amber{1.00f, 0.75f, 0.20f, 1.0f};
    const Color purple{0.70f, 0.40f, 1.00f, 1.0f};
    roll_ = NoteStyle{amber, amber, purple, 56.0, 56.0};

    const Color red{1.00f, 0.25f, 0.25f, 1.0f};
    mine_ = NoteStyle{red, red, red, 40.0, 40.0};
}

bool NoteSkin::init() {
    if (white_.valid() && receptor_.valid()) {
        return true;
    }

    if (glad_glGenTextures == nullptr) {
        std::cerr << "[NoteSkin] No GL context available; noteskin disabled\n";
        return false;
    }

    white_ = Texture::solid(Color{1.0f, 1.0f, 1.0f, 1.0f});
    receptor_ = Texture::solid(Color{0.85f, 0.90f, 1.00f, 0.9f});

    if (!white_.valid() || !receptor_.valid()) {
        std::cerr << "[NoteSkin] Failed to create placeholder textures (no GL context?)\n";
        return false;
    }
    return true;
}

void NoteSkin::shutdown() {
    white_.destroy();
    receptor_.destroy();
}

const NoteStyle& NoteSkin::style_for(NoteType type) const {
    switch (type) {
        case NoteType::Tap: return tap_;
        case NoteType::HoldHead: return hold_;
        case NoteType::RollHead: return roll_;
        case NoteType::Mine: return mine_;
    }
    return tap_;
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

} // namespace td
