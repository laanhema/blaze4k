#pragma once

// Cabinet theme textures by name (#89).
//
// Three layers:
//  1. parse_theme_manifest(): a pure, GL-free, never-throwing parser for
//     assets/theme/cabinet/manifest.json (untrusted input: size/count caps,
//     plain-file-name check, every rect bounds-checked against size_px).
//  2. Pure placement math: maps a screen-space *content box* (the manifest's
//     content_px, not the padded image that holds glows and shadows) to
//     destination rects + UVs for each manifest kind. Tests need no GL.
//  3. ThemeTextures / BitmapDigits: thin owners that load every PNG once at app
//     init and feed the pure math into GlQuadRenderer.
//
// Conventions:
//  - `s` is the layout scale, window_height / 720 (#91). The art is baked at
//    `texture_scale` (2x), so one image pixel is k = s / texture_scale screen px.
//  - Screen rects have a top-left origin; `tint` is a straight-alpha vertex
//    colour (multiplied with the texture), white by default.
//  - Fallback: when a PNG is missing/unreadable, failed to upload, or there is
//    no GL context, each draw issues one flat quad over the *content* rect with
//    theme_fallback_color() (a frame draws only its ring, so a banner is never
//    covered). An unknown name (which includes a missing/malformed manifest)
//    draws nothing for draw_sprite / draw_stretch_x / digits (no geometry is
//    known) and a flat quad over the caller's rect for the rect-based helpers.
//    Each unknown name is logged once.
//  - Presentation only: nothing here touches the music clock or judgment path.

#include <array>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "render/geometry.hpp"
#include "render/texture.hpp"

namespace blaze4k {

class GlQuadRenderer;

// Manifest `kind` values.
enum class ThemeKind { Fullscreen, Sprite, StretchX, Stretch, Slice3, Slice9, Frame, Tile };

// Integer image-pixel rect [x, y, w, h] (top-left origin).
struct PxRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

// slice3_px: left/right cap widths, measured from the image edges (padding included).
struct Slice3Px {
    int left = 0;
    int right = 0;
};

// slice9_px: border insets, measured from the image edges.
struct Slice9Px {
    int top = 0;
    int right = 0;
    int bottom = 0;
    int left = 0;
};

// One validated `textures` entry.
struct ThemeEntry {
    std::string file;
    ThemeKind kind = ThemeKind::Sprite;
    int width = 0;  // size_px[0]
    int height = 0; // size_px[1]
    PxRect content; // content_px (defaults to the full image when absent)
    std::optional<PxRect> hole;      // frame only
    std::optional<Slice3Px> slice3;  // slice3 only
    std::optional<Slice9Px> slice9;  // slice9 only
    // Tile with `"scale": "1x (screen pixels)"` (scanlines): one texel per
    // screen pixel, independent of `s`; sampled with GL_NEAREST.
    bool screen_pixel_tile = false;
};

// Characters the bitmap digit atlases provide, in glyph-table order.
inline constexpr std::string_view kDigitChars = "0123456789.%/ ";

// Index of `c` in kDigitChars, or -1 when unsupported. Pure.
[[nodiscard]] int digit_glyph_index(char c);

struct DigitGlyph {
    PxRect src;            // glyph rect in the atlas (image px)
    float origin_x = 0.0f; // draw at pen_x - origin_x
    float advance = 0.0f;  // then pen_x += advance
    bool present = false;
};

struct DigitFont {
    std::string file;
    std::array<DigitGlyph, 14> glyphs{};
};
static_assert(std::tuple_size_v<decltype(DigitFont::glyphs)> == kDigitChars.size(),
              "one glyph slot per supported digit character");

// Transparent string hash so maps keyed by std::string can be searched with a
// std::string_view without allocating (C++20 heterogeneous lookup).
struct ThemeStringHash {
    using is_transparent = void;
    [[nodiscard]] std::size_t operator()(std::string_view text) const noexcept {
        return std::hash<std::string_view>{}(text);
    }
};

template <typename T>
using ThemeNameMap = std::unordered_map<std::string, T, ThemeStringHash, std::equal_to<>>;
using ThemeNameSet = std::unordered_set<std::string, ThemeStringHash, std::equal_to<>>;

struct ThemeManifest {
    float texture_scale = 2.0f;
    ThemeNameMap<ThemeEntry> textures;
    ThemeNameMap<DigitFont> fonts;
};

// Parses manifest JSON text. Never throws: malformed JSON (or a non-object
// root) yields an empty manifest; each bad entry/glyph is skipped with one
// message appended to `warnings` (when non-null). Pure (no GL, no file I/O).
[[nodiscard]] ThemeManifest parse_theme_manifest(std::string_view json_text,
                                                 std::vector<std::string>* warnings = nullptr);

// ---------------------------------------------------------------------------------------------
// Pure placement math (no GL). Degenerate input (non-finite / non-positive
// sizes or scale, zero content dims) yields empty rects or std::nullopt.
// ---------------------------------------------------------------------------------------------

// One textured piece: where to draw and which part of the image to sample.
struct ThemePiece {
    Rect dst;
    UVRect uv;
};

// Content box for a sprite whose content top-left sits at `content_pos`, at
// k = s / texture_scale screen px per image px: {pos, C.w*k, C.h*k}.
[[nodiscard]] Rect sprite_content_rect(const ThemeEntry& entry, Vec2 content_pos, float k);

// Padded image quad for a content rect: per-axis scale kx = rect.w / C.w,
// ky = rect.h / C.h; the padding hangs outside by padding * (kx, ky).
[[nodiscard]] Rect content_to_image_rect(const ThemeEntry& entry, const Rect& content_rect);

// slice3: uniform k = rect.h / C.h (caps keep their aspect). Pieces are left
// cap, middle, right cap over the padded image rect. When the image rect is
// narrower than both caps, the caps shrink proportionally and the middle is 0.
[[nodiscard]] std::array<ThemePiece, 3> slice3_pieces(const ThemeEntry& entry,
                                                      const Rect& content_rect);

// slice9: border insets * k (k = s / texture_scale) over the padded image rect.
// Pieces are row-major (TL, T, TR, L, C, R, BL, B, BR). An axis smaller than
// its two borders shrinks them proportionally and the centre gets zero size.
[[nodiscard]] std::array<ThemePiece, 9> slice9_pieces(const ThemeEntry& entry,
                                                      const Rect& content_rect, float k);

// frame: the full image quad that places hole_px exactly over `hole_rect`
// (per-axis kx = hole.w / H.w, ky = hole.h / H.h).
[[nodiscard]] Rect frame_image_rect(const ThemeEntry& entry, const Rect& hole_rect);

// The four strips of `image_rect` around `hole_rect` (top, bottom, left,
// right), used by the frame fallback so a banner is never covered.
[[nodiscard]] std::array<Rect, 4> frame_ring_rects(const Rect& image_rect, const Rect& hole_rect);

// Where tiling starts: TopLeft puts a tile boundary on the rect's top-left;
// Bottom puts one on the rect bottom (life_stripes, "aligned to the track bottom").
enum class TileAnchor { TopLeft, Bottom };

// UVs that tile the whole image over `rect` (Repeat wrap). A tile is
// I.w*k x I.h*k screen px, or I.w x I.h for a screen_pixel_tile entry.
[[nodiscard]] UVRect tile_uv(const ThemeEntry& entry, const Rect& rect, float k,
                             TileAnchor anchor);

// Bottom-anchored fill of `bar_rect` by `fraction` (clamped to [0,1], NaN = 0):
// {bar.x, bar.y + bar.h*(1-f), bar.w, bar.h*f}. Pure geometry, entry-free.
[[nodiscard]] std::optional<Rect> fill_cropped_rect(const Rect& bar_rect, float fraction);

// The fill rect plus UVs cropped to the filled part of the content box's UV
// span (v in [1-f, 1] of that span), so the gradient stays fixed to the bar.
// f == 0 (or degenerate input) returns std::nullopt.
[[nodiscard]] std::optional<ThemePiece> fill_cropped_piece(const ThemeEntry& entry,
                                                           const Rect& bar_rect, float fraction);

// Flat-quad fallback colour: fullscreen = kNavyDeep * tint; every other kind =
// the tint at 25% of its alpha.
[[nodiscard]] Color theme_fallback_color(ThemeKind kind, Color tint);

// Per-texture load options: mipmaps for logo / grade_* / judgment_*; Repeat for
// tile kinds; Nearest for screen-pixel tiles (scanlines). Pure.
struct ThemeLoadOptions {
    bool mipmaps = false;
    Texture::Wrap wrap = Texture::Wrap::Clamp;
    Texture::Filter filter = Texture::Filter::Linear;
};
[[nodiscard]] ThemeLoadOptions theme_texture_options(std::string_view name, const ThemeEntry& entry);

// ---------------------------------------------------------------------------------------------
// Pure digit layout (no GL)
// ---------------------------------------------------------------------------------------------

enum class DigitAlign { Left, Centre, Right };

// Sum of advance * k over supported, present glyphs (others take zero width).
[[nodiscard]] float digits_width(const DigitFont& font, std::string_view text, float k);
// Starting pen x for an anchor x: x (Left), x - w/2 (Centre), x - w (Right).
[[nodiscard]] float digits_start_x(float x, float width, DigitAlign align);
// Glyph quad: {pen_x - origin_x*k, y, g.w*k, g.h*k}.
[[nodiscard]] Rect digit_glyph_rect(const DigitGlyph& glyph, float pen_x, float y, float k);
// Flat-fallback quad (no atlas): the advance cell {pen_x, y, advance*k, g.h*k},
// so neighbouring boxes do not overlap. Space draws nothing in the fallback.
[[nodiscard]] Rect digit_fallback_rect(const DigitGlyph& glyph, float pen_x, float y, float k);
// Glyph rect / atlas dims (full UV for non-positive atlas dims).
[[nodiscard]] UVRect digit_glyph_uv(const DigitGlyph& glyph, int atlas_w, int atlas_h);

// Bitmap digit string drawer for one atlas (digits_chrome / digits_white):
// draws `0-9 . % /` and space using each glyph's rect, origin_x and advance.
// Unsupported bytes take zero width and are logged once per font. Without a
// texture each non-space glyph draws as a flat advance-cell quad; without a
// font (unknown name or no manifest) nothing is drawn and that is logged once.
class BitmapDigits {
public:
    BitmapDigits() = default;
    BitmapDigits(const BitmapDigits&) = delete;
    BitmapDigits& operator=(const BitmapDigits&) = delete;
    BitmapDigits(BitmapDigits&&) noexcept = default;
    BitmapDigits& operator=(BitmapDigits&&) noexcept = default;

    // String width in screen px at layout scale `s`.
    [[nodiscard]] float measure(std::string_view text, float s) const;
    // Glyph height in screen px at layout scale `s` (tallest glyph).
    [[nodiscard]] float height(float s) const;
    // Draws `text` with its glyph tops at `y`; `x` is the anchor for `align`.
    void draw(GlQuadRenderer& renderer, std::string_view text, float x, float y, float s,
              DigitAlign align, Color tint = Color{}) const;

    // True when the manifest provided this font (geometry known).
    [[nodiscard]] bool valid() const { return font_present_; }
    [[nodiscard]] bool has_texture() const { return texture_.valid(); }

private:
    friend class ThemeTextures;

    void reset(std::string name);

    std::string name_;
    DigitFont font_;
    bool font_present_ = false;
    Texture texture_;
    int atlas_w_ = 0;
    int atlas_h_ = 0;
    float texture_scale_ = 2.0f;
    mutable bool warned_unsupported_ = false;
    mutable bool warned_missing_ = false;
};

// Owner of the Cabinet theme textures + digit fonts. Load once at app init
// (App::init); call shutdown() while the GL context is still alive.
class ThemeTextures {
public:
    ThemeTextures();
    ~ThemeTextures() = default;
    ThemeTextures(const ThemeTextures&) = delete;
    ThemeTextures& operator=(const ThemeTextures&) = delete;

    // assets/theme/cabinet from the cwd, else next to the executable; empty
    // when neither has a manifest.json.
    [[nodiscard]] static std::filesystem::path default_directory();

    // Parses dir/manifest.json and loads every PNG once. Returns true when the
    // manifest parsed and lists at least one texture or digit font (even if some
    // PNGs are missing, or headless, where nothing is uploaded and one line is
    // logged). Idempotent: calls shutdown() first.
    bool load(const std::filesystem::path& directory);
    // Releases every texture and forgets the manifest. Safe to repeat.
    void shutdown();

    [[nodiscard]] const ThemeEntry* entry(std::string_view name) const;
    // Content box size at layout scale `s`; {0,0} when unknown.
    [[nodiscard]] Vec2 content_size(std::string_view name, float s) const;

    // Sprite: content box top-left at `content_pos`.
    void draw_sprite(GlQuadRenderer& renderer, std::string_view name, Vec2 content_pos, float s,
                     Color tint = Color{}) const;
    // fullscreen / stretch: the content box fills `content_rect` (per-axis).
    void draw_stretch(GlQuadRenderer& renderer, std::string_view name, const Rect& content_rect,
                      Color tint = Color{}) const;
    // stretch_x: content box {x, y, width, C.h*k}; never stretched vertically.
    void draw_stretch_x(GlQuadRenderer& renderer, std::string_view name, float x, float y,
                        float width, float s, Color tint = Color{}) const;
    // slice3: content box at `content_rect`, middle stretched horizontally.
    void draw_slice3(GlQuadRenderer& renderer, std::string_view name, const Rect& content_rect,
                     Color tint = Color{}) const;
    // slice9: content box at `content_rect`, borders scaled by s / texture_scale.
    void draw_slice9(GlQuadRenderer& renderer, std::string_view name, const Rect& content_rect,
                     float s, Color tint = Color{}) const;
    // frame: hole_px placed over `hole_rect` (draw the banner first).
    void draw_frame(GlQuadRenderer& renderer, std::string_view name, const Rect& hole_rect,
                    Color tint = Color{}) const;
    // tile: covers `rect` with repeated tiles.
    void draw_tiled(GlQuadRenderer& renderer, std::string_view name, const Rect& rect, float s,
                    Color tint = Color{}, TileAnchor anchor = TileAnchor::TopLeft) const;
    // Bottom-anchored fill of `bar_rect` with UVs cropped so the gradient stays put.
    void draw_fill_cropped(GlQuadRenderer& renderer, std::string_view name, const Rect& bar_rect,
                           float fraction, Color tint = Color{}) const;

    [[nodiscard]] const BitmapDigits& digits_chrome() const { return digits_chrome_; }
    [[nodiscard]] const BitmapDigits& digits_white() const { return digits_white_; }

    [[nodiscard]] float texture_scale() const { return texture_scale_; }
    [[nodiscard]] std::size_t entry_count() const { return slots_.size(); }
    [[nodiscard]] std::size_t loaded_texture_count() const;

private:
    struct Slot {
        ThemeEntry entry;
        Texture texture;
    };

    // Looks up `name`; logs the first miss per name. Null when unknown.
    [[nodiscard]] const Slot* find(std::string_view name) const;
    // k = s / texture_scale, or 0 when `s` is not finite and positive.
    [[nodiscard]] float scale_k(float s) const;

    ThemeNameMap<Slot> slots_;
    float texture_scale_ = 2.0f;
    BitmapDigits digits_chrome_;
    BitmapDigits digits_white_;
    mutable ThemeNameSet warned_;
};

} // namespace blaze4k
