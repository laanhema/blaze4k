#pragma once

#include <array>
#include <filesystem>

#include "chart/note.hpp"
#include "render/geometry.hpp"
#include "timing/judgment_constants.hpp"
#include "render/texture.hpp"

namespace blaze4k {

// One drawable piece of skin art, drawn in a square box of `scale * note_size`
// centered on its screen position.
struct SkinSprite {
    const Texture* texture = nullptr; // nullptr = nothing to draw
    UVRect uv{};
    float rotation = 0.0f; // radians, clockwise on screen, about the box center
    float scale = 1.0f;
    Color tint{};
    BlendMode blend = BlendMode::Alpha;
};

// Hold/roll art: a body strip from the head to the tail plus an optional end cap
// just past the tail. Both are authored head-on-top (upscroll) and are flipped
// vertically for reverse (metrics.ini FlipHoldBodyWhenReverse).
struct HoldSprites {
    const Texture* body = nullptr;
    const Texture* cap = nullptr; // nullptr = no end cap
    float width_scale = 1.0f;     // strip/cap width relative to the note size
    // Body texture repeat length relative to the note size, anchored at the tail
    // so the pattern travels with the note. 0 stretches one copy over the body.
    float tile_scale = 0.0f;
    Color tint{};
};

// Cel sheet math, pure (no GL) so it is testable headless.
//
// Frame of `_Down Tap Note 16x16`: 8 quantization color bands (4th..64th; 192nd
// shares 64th) of two 16-frame rows, cycling over TapNoteAnimationLength = 2
// beats of song time (non-vivid: every note shows the same frame).
[[nodiscard]] UVRect cel_tap_frame(NoteQuantization quantization, double beat);
// Turns Down-facing art toward `column` (Left, Down, Up, Right): NoteSkin.lua
// `ret.Rotate`.
[[nodiscard]] float column_rotation(int column);
// Receptor brightness per Down Receptor.lua's beat-clocked diffuseramp: a white
// flash on the beat that settles to grey within a quarter beat.
[[nodiscard]] float cel_receptor_brightness(double beat);
// Mine core color: `_mine tex.png`'s red-to-brown gradient scrolled at
// TexVelocityX = +1 per second.
[[nodiscard]] Color cel_mine_core_color(double seconds);
// Mine spin: `_mine model.txt`'s bone turns once over keyframes 1..120, which
// play at MilkShape's 30 fps: one turn every 4 seconds.
[[nodiscard]] float cel_mine_rotation(double seconds);

// Cel explosion tweens (metrics.ini [GhostArrowDim]): W1..W5 play
// `diffusealpha,1.2;zoom,1.1;accelerate,0.15;zoom,1.0;diffusealpha,0`, and a
// hold/roll that ends OK plays HeldCommand, the same tween over 0.09 s on the
// W1 art.
inline constexpr double kCelTapExplosionSeconds = 0.15;
inline constexpr double kCelHeldExplosionSeconds = 0.09;

struct ExplosionTween {
    float zoom = 1.0f;
    float alpha = 0.0f;    // 0 = finished (or not started)
    float rotation = 0.0f; // radians, clockwise on screen
};

// Tween state `elapsed` seconds into a `duration`-long explosion: `accelerate`
// eases in (t^2), and the 1.2 start alpha holds full opacity a little longer.
[[nodiscard]] ExplosionTween cel_explosion_tween(double elapsed, double duration);

// Mine hit explosion (metrics.ini [GhostArrowDim] HitMineCommand): additive,
// `linear,0.2;rotationz,90;linear,0.2;rotationz,180;diffusealpha,0`, i.e. a
// steady half turn over 0.4 s that fades out during its second half.
inline constexpr double kCelMineExplosionSeconds = 0.4;
[[nodiscard]] ExplosionTween cel_mine_explosion_tween(double elapsed);

// Gameplay noteskin. `init` loads the ITG "Cel" skin (assets/noteskins/cel, see
// its README); if any texture is missing or invalid it logs and falls back to
// the procedural skin (render/note_art.*: white masks tinted by quantization).
// Either way the renderer only sees `SkinSprite`/`HoldSprites`.
class NoteSkin {
public:
    // Note box edge in screen pixels: StepMania's 64-unit arrow on its 480-line
    // theme scales to 96 px at 720p. Drawn in a `kColumnWidth` lane.
    static constexpr double kNoteSize = 96.0;
    static constexpr double kColumnWidth = 108.0;

    // Cel directory next to the working dir or the executable; empty if absent.
    [[nodiscard]] static std::filesystem::path default_directory();

    // Requires a GL context; returns false (and logs) if unavailable.
    bool init(const std::filesystem::path& cel_directory = default_directory());
    void shutdown();

    [[nodiscard]] bool using_cel() const { return cel_; }

    [[nodiscard]] SkinSprite receptor(int column, double beat) const;
    // Tap, hold-head and roll-head art (Cel redirects hold/roll heads to the tap).
    [[nodiscard]] SkinSprite head(NoteType type, int column, NoteQuantization quantization,
                                  double beat) const;
    // `active` = the hold is currently being held.
    [[nodiscard]] HoldSprites hold(NoteType type, bool active, NoteQuantization quantization) const;
    // Mine layers, back to front; a layer with a null texture is skipped.
    [[nodiscard]] std::array<SkinSprite, 2> mine(double seconds) const;
    // Receptor explosion for a tap graded `window` (Fantastic..WayOff = W1..W5),
    // `elapsed` seconds into a `duration`-long tween. Null texture when there is
    // nothing to draw (tween over, non-tap window, or no explosion art).
    [[nodiscard]] SkinSprite tap_explosion(int column, TapJudgment window, double elapsed,
                                           double duration) const;
    // Receptor glow shown while a hold/roll is active (HoldingOn/RollOn).
    [[nodiscard]] SkinSprite hold_explosion(int column) const;
    // Burst `elapsed` seconds after a mine was hit; same in every column (the
    // Cel explosion actor is not rotated).
    [[nodiscard]] SkinSprite mine_explosion(double elapsed) const;

    // ITG note color for the beat subdivision a note lands on (OpenITG
    // `NoteDisplay.cpp` denominator colors). Tints the procedural fallback so
    // timing reads at a glance; the Cel sheet bakes its own colors.
    [[nodiscard]] Color quantization_color(NoteQuantization quantization) const;

private:
    bool load_cel(const std::filesystem::path& directory);
    void load_cel_explosions(const std::filesystem::path& directory);
    bool build_procedural();

    bool cel_ = false;

    // Cel textures.
    Texture cel_tap_;
    Texture cel_receptor_;
    Texture cel_hold_body_[2]; // [inactive, active]
    Texture cel_hold_cap_[2];
    Texture cel_roll_body_[2];
    Texture cel_roll_cap_[2];
    Texture cel_mine_;
    Texture cel_mine_core_;
    // Explosions are optional: missing art only disables them.
    std::array<Texture, 5> cel_tap_explosion_; // W1..W5
    Texture cel_hold_explosion_;
    Texture cel_mine_explosion_;

    // Procedural fallback: direction baked into per-column masks.
    std::array<Texture, 4> tap_;
    std::array<Texture, 4> hold_;
    std::array<Texture, 4> roll_;
    std::array<Texture, 4> receptor_;
    Texture mine_;
    Texture body_;
};

} // namespace blaze4k
