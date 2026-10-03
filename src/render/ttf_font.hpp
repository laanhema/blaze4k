#pragma once

// TrueType text rendering with stb_truetype atlases (#90).
//
// Four layers:
//  1. FontFace: the bytes of one bundled .ttf. validate_sfnt() checks the sfnt
//     table directory before any stb call (stb_truetype is not hardened), then
//     the face caches the glyph index and advance of each baked code point.
//  2. FontAtlas: one per (font, pixel size). Bakes U+0020-U+007E, U+00A0-U+00FF
//     and U+0100-U+017F (319 code points) with stbtt_PackFontRanges at 2x2
//     oversampling, plus an in-atlas placeholder box. The coverage uploads as
//     white RGBA with the coverage in alpha, so tinting works like any sprite.
//  3. Pure layout: measure_text / for_each_text_quad turn UTF-8 into sheared
//     glyph quads. One shared glyph walker feeds both, so measure and draw can
//     never disagree. Tests run them against CPU-baked atlases with no GL.
//  4. TextRenderer: owns the 4 faces (theme::kFontFiles) and the atlases, and
//     draws a theme::TextStyle through GlQuadRenderer::draw_quad_points.
//
// Conventions:
//  - `s` is the layout scale, window_height / 720 (#91). Every TextStyle value
//    (size, tracking, the 2/3px shadow offset) is at 720p and is multiplied by s.
//    Atlases bake at size_px * s, so text stays crisp from 720p to 4K.
//  - `size_px` is the em size (CSS font-size), not the ascender-to-descender
//    height: bakes use STBTT_POINT_SIZE / stbtt_ScaleForMappingEmToPixels.
//  - `y` is the TOP of the line box (CSS line-height: normal): the baseline is
//    y + ascent. ascent() and line_height() are exposed for layout.
//  - `x` is the anchor for TextAlign (Left: start, Centre: middle, Right: end).
//  - Tracking is CSS letter-spacing: added after every visible glyph, the last
//    one included, and counted by measure. Kerning (GPOS/kern pairs) is applied.
//  - Colours are straight alpha, like the rest of the renderer.
//  - UTF-8 goes through unicode_text: zero-width code points draw nothing and
//    take no advance; malformed bytes decode to U+FFFD; a code point the font
//    lacks uses its ASCII fold, else the in-atlas placeholder box.
//  - Fallback: a missing or corrupt .ttf (or an atlas that cannot bake) logs
//    once and draws with the 5x7 bitmap font at about 0.7em cap height.
//  - Headless (no GL): measuring, truncation and coverage still work (font
//    metrics only); nothing bakes, and draws are no-ops.
//  - Presentation only: nothing here touches the music clock or judgment path.

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "render/geometry.hpp"
#include "render/texture.hpp"
#include "render/theme.hpp"

namespace blaze4k {

class GlQuadRenderer;

enum class TextAlign { Left, Centre, Right };

// Font files larger than this are rejected before reading (the bundled fonts are < 100 KB).
inline constexpr std::uintmax_t kMaxFontBytes = 16u << 20;

// U+0020-U+007E (95) + U+00A0-U+00FF (96) + U+0100-U+017F (128).
inline constexpr std::size_t kBakedGlyphCount = 319;

// Index of `cp` in the baked glyph table, or -1 when it is not baked. Pure.
[[nodiscard]] int baked_glyph_slot(char32_t cp);

// Checks the sfnt table directory of a TrueType font: size in [12, kMaxFontBytes],
// version 0x00010000 or 'true' (CFF 'OTTO' and collections 'ttcf' are rejected),
// 1 <= numTables <= 64, the directory and every table inside the data, the
// required tables (cmap, head, hhea, hmtx, loca, glyf, maxp) present, and
// head.unitsPerEm in [16, 16384]. On failure returns false and, when `error`
// is non-null, stores a readable reason. Pure; never reads out of bounds.
[[nodiscard]] bool validate_sfnt(std::span<const std::uint8_t> data, std::string* error);

// One validated TrueType font. Move-only; the stb font info points into the
// owned bytes, which never move (they live behind a unique_ptr).
class FontFace {
public:
    FontFace(FontFace&&) noexcept;
    FontFace& operator=(FontFace&&) noexcept;
    FontFace(const FontFace&) = delete;
    FontFace& operator=(const FontFace&) = delete;
    ~FontFace();

    // validate_sfnt, then stbtt_InitFont, then requires a glyph for 'A'.
    [[nodiscard]] static std::optional<FontFace> from_bytes(std::vector<std::uint8_t> bytes,
                                                            std::string* error);
    // Checks the size cap before reading, then from_bytes. Never throws.
    [[nodiscard]] static std::optional<FontFace> from_file(const std::filesystem::path& path,
                                                           std::string* error);

    [[nodiscard]] int units_per_em() const;
    // hhea metrics in font units (descent is negative).
    [[nodiscard]] int ascent() const;
    [[nodiscard]] int descent() const;
    [[nodiscard]] int line_gap() const;
    // Height of 'H' in font units, or 0.7em when the font has no 'H'.
    [[nodiscard]] int cap_height_units() const;
    // Font units -> pixels for an em size of `pixel_size` (0 for non-finite/<= 0 input).
    [[nodiscard]] float em_scale(float pixel_size) const;

    // Per baked slot (see baked_glyph_slot); out-of-range slots read as absent.
    [[nodiscard]] bool has(int slot) const;
    [[nodiscard]] int glyph_index(int slot) const;
    [[nodiscard]] int advance_units(int slot) const;
    // Pair kerning between two baked slots in font units (0 when either is absent).
    [[nodiscard]] int kern_units(int slot_a, int slot_b) const;

private:
    struct Impl;
    explicit FontFace(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;

    friend struct FontAtlas;
};

// One baked glyph: the quad relative to (pen, baseline) in pixels, its UVs, and
// whether it was baked. A glyph with no ink (space) has a zero-area quad.
struct AtlasGlyph {
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
    UVRect uv{};
    bool present = false;
};

// One (font, pixel size) atlas. bake() is CPU only (tests need no GL);
// upload() creates the texture and frees the CPU coverage.
struct FontAtlas {
    float pixel_size = 0.0f;
    int width = 0;
    int height = 0;
    int oversample = 2;
    std::vector<std::uint8_t> coverage; // width * height, freed after upload()
    std::array<AtlasGlyph, kBakedGlyphCount> glyphs{};
    AtlasGlyph placeholder{};
    Texture texture;

    // Packs the 319 code points (absent ones are skipped) plus the placeholder
    // box into an atlas at most `max_dim` px on each side (clamped to [256,
    // 4096]). The width starts at the power of two that fits the estimated glyph
    // area and doubles on failure; the height is cropped to the rows used
    // (a multiple of 4). If 2x2 oversampling cannot fit, retries once at 1x1.
    // Returns nullopt (with `error`) for a pixel size that is not finite, < 1 or
    // > 1024, or when nothing fits.
    [[nodiscard]] static std::optional<FontAtlas> bake(const FontFace& face, float pixel_size,
                                                       int max_dim, std::string* error);
    // Uploads the coverage as white RGBA (no mipmaps, clamp, linear). On
    // success frees `coverage` and returns true. Needs a GL context.
    bool upload();
};

// (255, 255, 255, c) per coverage texel c. Pure.
[[nodiscard]] std::vector<std::uint8_t> coverage_to_white_rgba(std::span<const std::uint8_t> coverage);

// A TextStyle resolved at one layout scale. All values in screen pixels.
struct TextLayout {
    float pixel_size = 0.0f;  // em size
    float tracking = 0.0f;    // after every visible glyph
    float shear = 0.0f;       // x += shear * (baseline - y)
    TextAlign align = TextAlign::Left;
    Color color{};
    float shadow_offset = 0.0f; // 0 = no shadow; else a kShadow copy this far down
};

// One glyph quad: corners TL, TR, BR, BL (screen px), UVs and straight-alpha colour.
struct GlyphQuad {
    std::array<Vec2, 4> corners{};
    UVRect uv{};
    Color color{};
};

// Pen advance of UTF-8 `text`: per visible glyph kern + advance + tracking.
// Ignores the italic overhang and the shadow offset (like a CSS box). Pure.
[[nodiscard]] float measure_text(const FontFace& face, std::string_view text, float pixel_size,
                                 float tracking);

// Emits the glyph quads of `text` laid out at (x, y) (y = line-box top), in
// draw order: every shadow quad first (when layout.shadow_offset > 0), then
// every colour quad. Glyphs with no ink (space) and quads with non-finite
// corners are skipped; empty text or non-finite x, y or size emit nothing.
// The atlas should be baked at layout.pixel_size (quads are scaled if not). Pure.
void for_each_text_quad(const FontFace& face, const FontAtlas& atlas, std::string_view text,
                        float x, float y, const TextLayout& layout,
                        const std::function<void(const GlyphQuad&)>& emit);

// window_height / 720, or 1 when window_height <= 0. Pure.
[[nodiscard]] float text_layout_scale(int window_height);

// Maps a theme style to a layout at scale `s`: size and tracking times s,
// shear = (italic ? kItalicShear : 0) + extra_shear, shadow Hard2/Hard3 ->
// 2s/3s. Pure.
[[nodiscard]] TextLayout resolve_text_layout(const theme::TextStyle& style, float s,
                                             TextAlign align, float extra_shear);

// Owner of the theme fonts and their atlases. App owns one: load() at init,
// set_window_height() before each render, shutdown() while GL is alive.
class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    // Loads each theme::kFontFiles entry from the cwd, else next to the
    // executable. Returns true when at least one font loaded. Idempotent
    // (calls shutdown() first). A missing/corrupt font logs one line.
    bool load();
    // Same, with every font path relative to `root` (tests).
    bool load(const std::filesystem::path& root);
    // Releases every atlas texture and face. Safe to repeat.
    void shutdown();

    // Sets the layout scale from the window height in pixels. O(1) when the
    // height is unchanged; otherwise re-bakes every (font, size) pair used by
    // theme::text::kAllStyles. Headless: records the height (measure uses the
    // new scale), bakes nothing and logs one line once.
    void set_window_height(int window_height);
    [[nodiscard]] float scale() const { return scale_; }

    // Width of `text` in `style` at the current scale (screen px).
    [[nodiscard]] float measure(std::string_view text, const theme::TextStyle& style) const;
    // Line-box top to baseline, and the full line height (screen px).
    [[nodiscard]] float ascent(const theme::TextStyle& style) const;
    [[nodiscard]] float line_height(const theme::TextStyle& style) const;
    // Measure-based "..." truncation (truncate_to_width with measure()).
    [[nodiscard]] std::string truncate(std::string_view text, const theme::TextStyle& style,
                                       float max_width) const;
    // True when every visible code point of `text` has a native glyph in
    // `font` (folds and placeholders do not count). When the font is not
    // loaded, answers for the bitmap fallback (font_covers_text).
    [[nodiscard]] bool covers_text(std::string_view text, theme::Font font) const;

    // Draws `text` with its line-box top at `y`; `x` is the anchor for `align`.
    // `extra_shear` adds a group skew (e.g. theme::text::kComboGroupShear).
    void draw(GlQuadRenderer& renderer, std::string_view text, float x, float y,
              const theme::TextStyle& style, TextAlign align = TextAlign::Left,
              float extra_shear = 0.0f);

    [[nodiscard]] bool font_available(theme::Font font) const;
    [[nodiscard]] std::size_t atlas_count() const;
    // The window height the atlases were last baked (or, headless, recorded) for; -1 before any.
    [[nodiscard]] int baked_height() const { return baked_height_; }

private:
    struct AtlasSlot {
        theme::Font font = theme::Font::SairaBold;
        float size_px = 0.0f;               // at 720p
        std::optional<FontAtlas> atlas;     // nullopt: the bake failed (not retried)
    };

    bool load_paths(const std::array<std::filesystem::path, theme::kFontCount>& paths);
    // Queries GL_MAX_TEXTURE_SIZE once (GL only) and caps it at 4096.
    void ensure_max_texture_size();
    [[nodiscard]] const FontFace* face(theme::Font font) const;
    // Finds the atlas for (font, size_px), baking it on a miss (GL only). Null
    // when unavailable.
    [[nodiscard]] const FontAtlas* atlas_for(theme::Font font, float size_px, bool lazy);
    AtlasSlot& bake_slot(theme::Font font, float size_px);
    void draw_bitmap_fallback(GlQuadRenderer& renderer, const FontFace* face,
                              std::string_view text, float x, float y,
                              const TextLayout& layout) const;

    std::array<std::optional<FontFace>, theme::kFontCount> faces_{};
    std::vector<AtlasSlot> atlases_;
    int baked_height_ = -1;
    float scale_ = 1.0f;
    int max_texture_size_ = 0; // 0 until queried (GL only)
    bool warned_headless_ = false;
    bool warned_atlas_cap_ = false;
    std::array<bool, theme::kFontCount> warned_fallback_{};
};

} // namespace blaze4k
