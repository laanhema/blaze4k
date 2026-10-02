#include "gameplay/noteskin.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <vector>

#include <glad/glad.h>

#include "data/data_paths.hpp"
#include "render/note_art.hpp"

namespace blaze4k {

namespace {

// Procedural mask resolution: note heads draw at 56px in a 720p frame, so 64px
// keeps the procedural edges crisp without a meaningful memory cost.
constexpr int kMaskSize = 64;

// Cel files, under their Noteskin Workshop names so customization PNGs drop in.
constexpr const char* kCelTapFile = "_Down Tap Note 16x16 (doubleres).png";
constexpr const char* kCelReceptorFile = "_Down Receptor tex 4x1 (res 256x64).png";
constexpr const char* kCelHoldBodyFiles[2] = {"Down Hold Body Inactive (res 64x256).png",
                                              "Down Hold Body Active (res 64x256).png"};
constexpr const char* kCelHoldCapFiles[2] = {"Down Hold BottomCap Inactive (res 64x64).png",
                                             "Down Hold BottomCap Active (res 64x64).png"};
constexpr const char* kCelRollBodyFiles[2] = {"Down Roll Body Inactive (res 64x256).png",
                                              "Down Roll Body active (res 64x256).png"};
constexpr const char* kCelRollCapFiles[2] = {"Down Roll BottomCap Inactive (res 64x64).png",
                                             "Down Roll BottomCap Active (res 64x64).png"};
constexpr const char* kCelMineFile = "_mine tex.png";
constexpr const char* kCelExplosionDir = "explosions";
constexpr const char* kCelTapExplosionFiles[5] = {
    "Down Tap Explosion Dim W1 (res 125x125).png", "Down Tap Explosion Dim W2 (res 125x125).png",
    "Down Tap Explosion Dim W3 (res 125x125).png", "Down Tap Explosion Dim W4 (res 125x125).png",
    "Down Tap Explosion Dim W5 (res 125x125).png"};
constexpr const char* kCelHoldExplosionFile = "down hold explosion (res 125x125).png";
// Not part of the Workshop skin: StepMania's `common` fallback noteskin art.
constexpr const char* kCelMineExplosionFile = "Fallback HitMine Explosion.png";

// `_Down Tap Note 16x16`: 16x16 frame grid; each quantization owns two rows
// (metrics.ini NoteColorTextureCoordSpacingY = 0.125).
constexpr int kCelSheetGrid = 16;
constexpr int kCelColorBands = 8;
constexpr int kCelFramesPerColor = 32;
constexpr double kCelTapAnimationBeats = 2.0;

// Receptor.lua: effectcolor1 0.1, effectcolor2 1, ramping over a quarter beat
// to the half-way color, (0.1 + 1) / 2.
constexpr float kCelReceptorRest = 0.55f;
constexpr double kCelReceptorFlashBeats = 0.25;

// `_mine model.txt` geometry, in 64-unit arrow space: ring outer radius 29.42
// (mapped to the texture's top half), glowing core radius 16.
constexpr float kCelMineRingScale = static_cast<float>(2.0 * 29.422066 / 64.0);
constexpr float kCelMineCoreScale = static_cast<float>(2.0 * 16.0 / 64.0);
constexpr double kCelMineSpinSeconds = 120.0 / 30.0;

// Explosion art is authored at "res 125x125" around a 64-unit arrow.
constexpr float kCelExplosionScale = static_cast<float>(125.0 / 64.0);
// GhostArrowDim W*Command: zoom 1.1 -> 1.0 and diffusealpha 1.2 -> 0.
constexpr double kCelExplosionStartZoom = 1.1;
constexpr double kCelExplosionStartAlpha = 1.2;
// `Fallback HitMine Explosion.png` has no res tag: 128x128 units.
constexpr float kCelMineExplosionScale = static_cast<float>(128.0 / 64.0);

// Procedural mine: 40px in a 56px note box, scaled with the box.
constexpr float kProceduralMineScale = static_cast<float>(40.0 / 56.0);

constexpr Color kWhite{1.0f, 1.0f, 1.0f, 1.0f};

double fract(double value) {
    return value - std::floor(value);
}

Color lerp(Color a, Color b, float t) {
    return Color{a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
                 a.a + (b.a - a.a) * t};
}

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

const Texture& select(const std::array<Texture, 4>& textures, int column) {
    if (column < 0 || column >= static_cast<int>(textures.size())) {
        return textures[2]; // Up is the canonical direction.
    }
    return textures[static_cast<std::size_t>(column)];
}

// Column tint for the procedural receptors (subtle per-column hue).
Color procedural_receptor_tint(int column) {
    static const std::array<Color, 4> kColumnTints = {
        Color{1.00f, 1.00f, 1.00f, 1.0f},
        Color{0.85f, 0.95f, 1.00f, 1.0f},
        Color{1.00f, 0.90f, 0.95f, 1.0f},
        Color{0.95f, 1.00f, 0.90f, 1.0f},
    };
    if (column < 0 || column >= static_cast<int>(kColumnTints.size())) {
        return kWhite;
    }
    return kColumnTints[static_cast<std::size_t>(column)];
}

} // namespace

UVRect cel_tap_frame(NoteQuantization quantization, double beat) {
    const int band = std::clamp(static_cast<int>(quantization), 0, kCelColorBands - 1);
    const double phase = fract(beat / kCelTapAnimationBeats);
    const int frame =
        std::min(static_cast<int>(phase * kCelFramesPerColor), kCelFramesPerColor - 1);
    const int col = frame % kCelSheetGrid;
    const int row = band * (kCelFramesPerColor / kCelSheetGrid) + frame / kCelSheetGrid;
    const float cell = 1.0f / static_cast<float>(kCelSheetGrid);
    return UVRect{static_cast<float>(col) * cell, static_cast<float>(row) * cell,
                  static_cast<float>(col + 1) * cell, static_cast<float>(row + 1) * cell};
}

float column_rotation(int column) {
    constexpr float kPi = std::numbers::pi_v<float>;
    switch (column) {
        case 0: return kPi * 0.5f;  // Left: +90
        case 1: return 0.0f;        // Down: art as authored
        case 2: return kPi;         // Up: 180
        case 3: return -kPi * 0.5f; // Right: -90
        default: return 0.0f;
    }
}

float cel_receptor_brightness(double beat) {
    const double phase = fract(beat);
    if (phase >= kCelReceptorFlashBeats) {
        return kCelReceptorRest;
    }
    const float t = static_cast<float>(phase / kCelReceptorFlashBeats);
    return 1.0f + (kCelReceptorRest - 1.0f) * t;
}

Color cel_mine_core_color(double seconds) {
    // `_mine tex.png` bottom half (v = 0.75 row): pure red for u < 0.1875,
    // blending to brown by u = 0.3125, brown to the end.
    constexpr Color kRed{1.0f, 0.0f, 0.0f, 1.0f};
    constexpr Color kBrown{137.0f / 255.0f, 56.0f / 255.0f, 0.0f, 1.0f};
    const double u = fract(seconds);
    const float t = static_cast<float>(std::clamp((u - 0.1875) / 0.125, 0.0, 1.0));
    return lerp(kRed, kBrown, t);
}

float cel_mine_rotation(double seconds) {
    return static_cast<float>(fract(seconds / kCelMineSpinSeconds) * 2.0 * std::numbers::pi);
}

ExplosionTween cel_explosion_tween(double elapsed, double duration) {
    if (duration <= 0.0 || elapsed < 0.0 || elapsed >= duration) {
        return ExplosionTween{};
    }
    const double eased = (elapsed / duration) * (elapsed / duration); // accelerate
    const double zoom = kCelExplosionStartZoom + (1.0 - kCelExplosionStartZoom) * eased;
    const double alpha = kCelExplosionStartAlpha * (1.0 - eased);
    return ExplosionTween{static_cast<float>(zoom), static_cast<float>(std::min(alpha, 1.0))};
}

ExplosionTween cel_mine_explosion_tween(double elapsed) {
    if (elapsed < 0.0 || elapsed >= kCelMineExplosionSeconds) {
        return ExplosionTween{};
    }
    const double half = kCelMineExplosionSeconds * 0.5;
    const double alpha = elapsed < half ? 1.0 : 1.0 - (elapsed - half) / half;
    const double rotation = std::numbers::pi * elapsed / kCelMineExplosionSeconds;
    return ExplosionTween{1.0f, static_cast<float>(alpha), static_cast<float>(rotation)};
}

std::filesystem::path NoteSkin::default_directory() {
    const std::filesystem::path relative =
        std::filesystem::path("assets") / "noteskins" / "cel" / kCelTapFile;
    const std::filesystem::path tap = resolve_first_existing({
        relative,
        default_executable_dir() / relative,
    });
    return tap.empty() ? std::filesystem::path{} : tap.parent_path();
}

bool NoteSkin::init(const std::filesystem::path& cel_directory) {
    if (cel_ || receptor_[2].valid()) {
        return true;
    }

    if (glad_glGenTextures == nullptr) {
        std::cerr << "[NoteSkin] No GL context available; noteskin disabled\n";
        return false;
    }

    if (load_cel(cel_directory)) {
        cel_ = true;
        std::cout << "[NoteSkin] Loaded Cel noteskin from " << cel_directory.string() << "\n";
        return true;
    }
    std::cerr << "[NoteSkin] Cel noteskin unavailable; using the procedural fallback skin\n";
    return build_procedural();
}

bool NoteSkin::load_cel(const std::filesystem::path& directory) {
    if (directory.empty()) {
        std::cerr << "[NoteSkin] Cel noteskin directory not found (assets/noteskins/cel)\n";
        return false;
    }

    bool ok = true;
    // Mipmapped: the art is authored at 2-8x the on-screen note size.
    const auto load = [&](Texture& texture, const char* file) {
        texture = Texture::from_file((directory / file).string(), true);
        if (!texture.valid()) {
            std::cerr << "[NoteSkin] Missing or invalid Cel texture: " << file << "\n";
            ok = false;
        }
    };
    load(cel_tap_, kCelTapFile);
    load(cel_receptor_, kCelReceptorFile);
    for (int active = 0; active < 2; ++active) {
        load(cel_hold_body_[active], kCelHoldBodyFiles[active]);
        load(cel_hold_cap_[active], kCelHoldCapFiles[active]);
        load(cel_roll_body_[active], kCelRollBodyFiles[active]);
        load(cel_roll_cap_[active], kCelRollCapFiles[active]);
    }
    load(cel_mine_, kCelMineFile);
    cel_mine_core_ = upload_mask(make_disc_rgba(kMaskSize));
    ok = ok && cel_mine_core_.valid();

    if (!ok) {
        shutdown();
        return false;
    }
    load_cel_explosions(directory / kCelExplosionDir);
    return true;
}

void NoteSkin::load_cel_explosions(const std::filesystem::path& directory) {
    const auto load = [&](Texture& texture, const char* file) {
        texture = Texture::from_file((directory / file).string(), true);
        if (!texture.valid()) {
            std::cerr << "[NoteSkin] Missing or invalid Cel explosion: " << file
                      << " (that explosion is disabled)\n";
        }
    };
    for (std::size_t i = 0; i < cel_tap_explosion_.size(); ++i) {
        load(cel_tap_explosion_[i], kCelTapExplosionFiles[i]);
    }
    load(cel_hold_explosion_, kCelHoldExplosionFile);
    load(cel_mine_explosion_, kCelMineExplosionFile);
}

bool NoteSkin::build_procedural() {
    for (std::size_t column = 0; column < directions().size(); ++column) {
        const ArrowDirection dir = directions()[column];
        tap_[column] = upload_mask(make_arrow_rgba(kMaskSize, dir));
        hold_[column] = upload_mask(make_hold_head_rgba(kMaskSize, dir));
        roll_[column] = upload_mask(make_roll_head_rgba(kMaskSize, dir));
        receptor_[column] = upload_mask(make_receptor_rgba(kMaskSize, dir));
    }
    mine_ = upload_mask(make_mine_rgba(kMaskSize));
    body_ = upload_mask(make_body_rgba(kMaskSize));

    if (!receptor_[2].valid()) {
        std::cerr << "[NoteSkin] Failed to create noteskin textures (no GL context?)\n";
        return false;
    }
    return true;
}

void NoteSkin::shutdown() {
    cel_ = false;
    for (Texture* texture : {&cel_tap_, &cel_receptor_, &cel_mine_, &cel_mine_core_,
                             &cel_hold_explosion_, &cel_mine_explosion_, &mine_, &body_}) {
        texture->destroy();
    }
    for (Texture& texture : cel_tap_explosion_) {
        texture.destroy();
    }
    for (int active = 0; active < 2; ++active) {
        cel_hold_body_[active].destroy();
        cel_hold_cap_[active].destroy();
        cel_roll_body_[active].destroy();
        cel_roll_cap_[active].destroy();
    }
    for (std::array<Texture, 4>* textures : {&tap_, &hold_, &roll_, &receptor_}) {
        for (Texture& texture : *textures) {
            texture.destroy();
        }
    }
}

SkinSprite NoteSkin::receptor(int column, double beat) const {
    if (cel_) {
        const float brightness = cel_receptor_brightness(beat);
        return SkinSprite{&cel_receptor_, UVRect{0.0f, 0.0f, 0.25f, 1.0f}, column_rotation(column),
                          1.0f, Color{brightness, brightness, brightness, 1.0f}};
    }
    return SkinSprite{&select(receptor_, column), UVRect{}, 0.0f, 1.0f,
                      procedural_receptor_tint(column)};
}

SkinSprite NoteSkin::head(NoteType type, int column, NoteQuantization quantization,
                          double beat) const {
    if (cel_) {
        return SkinSprite{&cel_tap_, cel_tap_frame(quantization, beat), column_rotation(column),
                          1.0f, kWhite};
    }
    const std::array<Texture, 4>& textures =
        type == NoteType::HoldHead ? hold_ : (type == NoteType::RollHead ? roll_ : tap_);
    return SkinSprite{&select(textures, column), UVRect{}, 0.0f, 1.0f,
                      quantization_color(quantization)};
}

HoldSprites NoteSkin::hold(NoteType type, bool active, NoteQuantization quantization) const {
    if (cel_) {
        const int index = active ? 1 : 0;
        const bool roll = type == NoteType::RollHead;
        return HoldSprites{roll ? &cel_roll_body_[index] : &cel_hold_body_[index],
                           roll ? &cel_roll_cap_[index] : &cel_hold_cap_[index],
                           1.0f, 4.0f, kWhite, // 64x256 body, 64x64 cap per 64-unit arrow
                           kCelHoldBodyStopFromTail};
    }
    return HoldSprites{&body_, nullptr, 0.6f, 0.0f,
                       multiply(with_alpha(kWhite, 0.65f), quantization_color(quantization))};
}

std::array<SkinSprite, 2> NoteSkin::mine(double seconds) const {
    if (cel_) {
        return {
            SkinSprite{&cel_mine_core_, UVRect{}, 0.0f, kCelMineCoreScale,
                       cel_mine_core_color(seconds)},
            SkinSprite{&cel_mine_, UVRect{0.0f, 0.0f, 1.0f, 0.5f}, cel_mine_rotation(seconds),
                       kCelMineRingScale, kWhite},
        };
    }
    return {
        SkinSprite{&mine_, UVRect{}, 0.0f, kProceduralMineScale, Color{1.00f, 0.25f, 0.25f, 1.0f}},
        SkinSprite{},
    };
}

SkinSprite NoteSkin::tap_explosion(int column, TapJudgment window, double elapsed,
                                   double duration) const {
    const auto index = static_cast<std::size_t>(window);
    if (!cel_ || index >= cel_tap_explosion_.size() || !cel_tap_explosion_[index].valid()) {
        return SkinSprite{};
    }
    const ExplosionTween tween = cel_explosion_tween(elapsed, duration);
    if (tween.alpha <= 0.0f) {
        return SkinSprite{};
    }
    return SkinSprite{&cel_tap_explosion_[index], UVRect{}, column_rotation(column),
                      kCelExplosionScale * tween.zoom, with_alpha(kWhite, tween.alpha)};
}

SkinSprite NoteSkin::hold_explosion(int column) const {
    if (!cel_ || !cel_hold_explosion_.valid()) {
        return SkinSprite{};
    }
    return SkinSprite{&cel_hold_explosion_, UVRect{}, column_rotation(column), kCelExplosionScale,
                      kWhite};
}

SkinSprite NoteSkin::mine_explosion(double elapsed) const {
    if (!cel_ || !cel_mine_explosion_.valid()) {
        return SkinSprite{};
    }
    const ExplosionTween tween = cel_mine_explosion_tween(elapsed);
    if (tween.alpha <= 0.0f) {
        return SkinSprite{};
    }
    return SkinSprite{&cel_mine_explosion_, UVRect{}, tween.rotation, kCelMineExplosionScale,
                      with_alpha(kWhite, tween.alpha), BlendMode::Add};
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
    return kWhite;
}

} // namespace blaze4k
