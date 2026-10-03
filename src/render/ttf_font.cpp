#include "render/ttf_font.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <system_error>
#include <utility>

#include <glad/glad.h>
#include <stb_rect_pack.h>
#include <stb_truetype.h>

#include "data/data_paths.hpp"
#include "render/bitmap_font.hpp"
#include "render/gl_quad_renderer.hpp"
#include "render/theme_layout.hpp"
#include "render/unicode_text.hpp"

namespace blaze4k {

namespace {

constexpr std::size_t kMaxAtlases = 32;
constexpr int kMinAtlasDim = 256;
constexpr int kMaxAtlasDim = 4096;
constexpr float kMinPixelSize = 1.0f;
constexpr float kMaxPixelSize = 1024.0f;
constexpr float kPlaceholderAdvanceEm = 0.6f;
constexpr float kPlaceholderLeftEm = 0.08f;
constexpr float kPlaceholderWidthEm = 0.44f; // ink from 0.08em to 0.52em
constexpr float kPlaceholderStrokeEm = 0.06f;
constexpr float kDefaultCapHeightEm = 0.7f;
// Bitmap fallback: pixel = P / 10 (7-row cap ~0.7em) in a 1.2em line box.
constexpr float kFallbackPixelPerEm = 0.1f;
constexpr float kFallbackLineEm = 1.2f;

// The three baked ranges: first code point, count, first slot.
struct BakedRange {
    char32_t first;
    int count;
    int first_slot;
};
constexpr std::array<BakedRange, 3> kBakedRanges = {{
    {0x0020, 95, 0},
    {0x00A0, 96, 95},
    {0x0100, 128, 191},
}};
static_assert(95 + 96 + 128 == kBakedGlyphCount, "baked ranges must cover every slot");

[[nodiscard]] char32_t slot_code_point(int slot) {
    for (const BakedRange& range : kBakedRanges) {
        if (slot >= range.first_slot && slot < range.first_slot + range.count) {
            return range.first + static_cast<char32_t>(slot - range.first_slot);
        }
    }
    return 0;
}

[[nodiscard]] bool read_u16(std::span<const std::uint8_t> data, std::size_t offset,
                            std::uint16_t& out) {
    if (offset > data.size() || data.size() - offset < 2) {
        return false;
    }
    out = static_cast<std::uint16_t>((data[offset] << 8) | data[offset + 1]);
    return true;
}

[[nodiscard]] bool read_u32(std::span<const std::uint8_t> data, std::size_t offset,
                            std::uint32_t& out) {
    if (offset > data.size() || data.size() - offset < 4) {
        return false;
    }
    out = (static_cast<std::uint32_t>(data[offset]) << 24) |
          (static_cast<std::uint32_t>(data[offset + 1]) << 16) |
          (static_cast<std::uint32_t>(data[offset + 2]) << 8) |
          static_cast<std::uint32_t>(data[offset + 3]);
    return true;
}

[[nodiscard]] constexpr std::uint32_t tag(const char (&name)[5]) {
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(name[0])) << 24) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(name[1])) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(name[2])) << 8) |
           static_cast<std::uint32_t>(static_cast<unsigned char>(name[3]));
}

[[nodiscard]] std::string tag_text(std::uint32_t value) {
    std::string text(4, '?');
    for (int i = 0; i < 4; ++i) {
        const auto c = static_cast<char>((value >> (24 - 8 * i)) & 0xFFu);
        text[static_cast<std::size_t>(i)] = (c >= 0x20 && c <= 0x7E) ? c : '?';
    }
    return text;
}

bool fail(std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
    return false;
}

[[nodiscard]] bool finite_positive(float value) {
    return std::isfinite(value) && value > 0.0f;
}

[[nodiscard]] const char* font_name(theme::Font font) {
    switch (font) {
    case theme::Font::Audiowide:
        return "Audiowide";
    case theme::Font::SairaMedium:
        return "SairaMedium";
    case theme::Font::SairaBold:
        return "SairaBold";
    case theme::Font::SairaExtraBold:
        return "SairaExtraBold";
    case theme::Font::Count:
        break;
    }
    return "?";
}

[[nodiscard]] std::size_t font_slot(theme::Font font) {
    return static_cast<std::size_t>(font);
}

// Baked slot for a visible code point: native glyph, else its ASCII fold,
// else -1 (the placeholder box).
[[nodiscard]] int resolve_slot(const FontFace& face, char32_t cp) {
    const int slot = baked_glyph_slot(cp);
    if (slot >= 0 && face.has(slot)) {
        return slot;
    }
    const char folded = fold_to_ascii(cp);
    if (folded != '\0') {
        const int folded_slot = baked_glyph_slot(static_cast<unsigned char>(folded));
        if (folded_slot >= 0 && face.has(folded_slot)) {
            return folded_slot;
        }
    }
    return -1;
}

// The one glyph walker shared by measure and draw: zero-width code points are
// skipped (no advance, kerning predecessor unchanged); every visible code point
// yields (cp, slot or -1 for the placeholder, kern px, advance px).
template <typename Fn>
void walk_glyphs(const FontFace& face, std::string_view text, float pixel_size, float k_em,
                 Fn&& fn) {
    int previous = -2; // -2: no predecessor; -1: the placeholder
    std::size_t pos = 0;
    while (pos < text.size()) {
        const char32_t cp = next_code_point(text, pos);
        if (is_zero_width(cp)) {
            continue;
        }
        const int slot = resolve_slot(face, cp);
        const float kern = (previous >= 0 && slot >= 0)
                               ? static_cast<float>(face.kern_units(previous, slot)) * k_em
                               : 0.0f;
        const float advance = slot >= 0 ? static_cast<float>(face.advance_units(slot)) * k_em
                                        : kPlaceholderAdvanceEm * pixel_size;
        fn(cp, slot, kern, advance);
        previous = slot;
    }
}

[[nodiscard]] float sanitize(float value) {
    return std::isfinite(value) ? value : 0.0f;
}

[[nodiscard]] float aligned_start(float x, float width, TextAlign align) {
    switch (align) {
    case TextAlign::Centre:
        return x - width * 0.5f;
    case TextAlign::Right:
        return x - width;
    case TextAlign::Left:
        break;
    }
    return x;
}

[[nodiscard]] Color shadow_color(Color color) {
    return with_alpha(theme::color::kShadow, color.a);
}

// Bitmap fallback width when the face is missing: 6 * (P / 10) + tracking per
// visible code point.
[[nodiscard]] float bitmap_measure(std::string_view text, float pixel_size, float tracking) {
    std::size_t visible = 0;
    std::size_t pos = 0;
    while (pos < text.size()) {
        if (!is_zero_width(next_code_point(text, pos))) {
            ++visible;
        }
    }
    return static_cast<float>(visible) * (6.0f * kFallbackPixelPerEm * pixel_size + tracking);
}

[[nodiscard]] bool gl_present() {
    return glad_glGenTextures != nullptr;
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Pure helpers
// ---------------------------------------------------------------------------------------------

int baked_glyph_slot(char32_t cp) {
    for (const BakedRange& range : kBakedRanges) {
        if (cp >= range.first && cp < range.first + static_cast<char32_t>(range.count)) {
            return range.first_slot + static_cast<int>(cp - range.first);
        }
    }
    return -1;
}

bool validate_sfnt(std::span<const std::uint8_t> data, std::string* error) {
    if (data.size() < 12) {
        return fail(error, "file is too small to be a font (" + std::to_string(data.size()) +
                               " bytes)");
    }
    if (data.size() > kMaxFontBytes) {
        return fail(error, "file is larger than the 16 MiB font cap");
    }
    std::uint32_t version = 0;
    std::uint16_t table_count = 0;
    if (!read_u32(data, 0, version) || !read_u16(data, 4, table_count)) {
        return fail(error, "unreadable sfnt header");
    }
    if (version == tag("OTTO")) {
        return fail(error, "CFF (OTTO) outlines are not supported");
    }
    if (version == tag("ttcf")) {
        return fail(error, "font collections (ttcf) are not supported");
    }
    if (version != 0x00010000u && version != tag("true")) {
        return fail(error, "not a TrueType font (bad sfnt version)");
    }
    if (table_count < 1 || table_count > 64) {
        return fail(error, "implausible table count " + std::to_string(table_count));
    }
    if (12u + 16u * static_cast<std::size_t>(table_count) > data.size()) {
        return fail(error, "table directory extends past the end of the file");
    }

    struct Required {
        std::uint32_t tag;
        std::uint32_t min_length;
        std::uint32_t offset = 0;
        bool found = false;
    };
    std::array<Required, 7> required = {{
        {tag("cmap"), 4},
        {tag("head"), 54},
        {tag("hhea"), 36},
        {tag("hmtx"), 4},
        {tag("loca"), 2},
        {tag("glyf"), 0},
        {tag("maxp"), 6},
    }};
    for (std::size_t i = 0; i < table_count; ++i) {
        const std::size_t record = 12 + 16 * i;
        std::uint32_t table_tag = 0;
        std::uint32_t offset = 0;
        std::uint32_t length = 0;
        if (!read_u32(data, record, table_tag) || !read_u32(data, record + 8, offset) ||
            !read_u32(data, record + 12, length)) {
            return fail(error, "unreadable table record");
        }
        if (static_cast<std::uint64_t>(offset) + static_cast<std::uint64_t>(length) >
            static_cast<std::uint64_t>(data.size())) {
            return fail(error, "table '" + tag_text(table_tag) +
                                   "' extends past the end of the file");
        }
        for (Required& entry : required) {
            if (entry.tag == table_tag) {
                // stb reads the first match; a second one would go unchecked.
                if (entry.found) {
                    return fail(error, "duplicate table '" + tag_text(table_tag) + "'");
                }
                if (length < entry.min_length) {
                    return fail(error, "table '" + tag_text(table_tag) + "' is truncated");
                }
                entry.found = true;
                entry.offset = offset;
            }
        }
    }
    for (const Required& entry : required) {
        if (!entry.found) {
            return fail(error, "missing required table '" + tag_text(entry.tag) + "'");
        }
    }
    std::uint16_t units_per_em = 0;
    if (!read_u16(data, static_cast<std::size_t>(required[1].offset) + 18, units_per_em) ||
        units_per_em < 16 || units_per_em > 16384) {
        return fail(error, "implausible unitsPerEm " + std::to_string(units_per_em));
    }
    return true;
}

std::vector<std::uint8_t> coverage_to_white_rgba(std::span<const std::uint8_t> coverage) {
    std::vector<std::uint8_t> rgba(coverage.size() * 4u, 255);
    for (std::size_t i = 0; i < coverage.size(); ++i) {
        rgba[i * 4 + 3] = coverage[i];
    }
    return rgba;
}

float text_layout_scale(int window_width, int window_height) {
    return theme::layout_scale_factor(window_width, window_height);
}

TextLayout resolve_text_layout(const theme::TextStyle& style, float s, TextAlign align,
                               float extra_shear) {
    TextLayout layout;
    layout.pixel_size = style.size_px * s;
    layout.tracking = style.tracking_px * s;
    layout.shear = (style.italic ? theme::kItalicShear : 0.0f) + extra_shear;
    layout.align = align;
    layout.color = style.color;
    switch (style.shadow) {
    case theme::Shadow::Hard2:
        layout.shadow_offset = 2.0f * s;
        break;
    case theme::Shadow::Hard3:
        layout.shadow_offset = 3.0f * s;
        break;
    case theme::Shadow::None:
        layout.shadow_offset = 0.0f;
        break;
    }
    return layout;
}

float bitmap_fallback_baseline(const FontFace* face, float pixel_size) {
    if (!finite_positive(pixel_size)) {
        return 0.0f;
    }
    if (face != nullptr) {
        return static_cast<float>(face->ascent()) * face->em_scale(pixel_size);
    }
    return (kFallbackLineEm * pixel_size + 7.0f * kFallbackPixelPerEm * pixel_size) * 0.5f;
}

// ---------------------------------------------------------------------------------------------
// FontFace
// ---------------------------------------------------------------------------------------------

struct FontFace::Impl {
    std::vector<std::uint8_t> bytes;
    stbtt_fontinfo info{};
    std::array<int, kBakedGlyphCount> glyph{};
    std::array<int, kBakedGlyphCount> advance{};
    int units_per_em = 0;
    int ascent = 0;
    int descent = 0;
    int line_gap = 0;
    int cap_height = 0;
};

FontFace::FontFace(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
FontFace::FontFace(FontFace&&) noexcept = default;
FontFace& FontFace::operator=(FontFace&&) noexcept = default;
FontFace::~FontFace() = default;

std::optional<FontFace> FontFace::from_bytes(std::vector<std::uint8_t> bytes, std::string* error) {
    if (!validate_sfnt(bytes, error)) {
        return std::nullopt;
    }
    auto impl = std::make_unique<Impl>();
    impl->bytes = std::move(bytes);
    const unsigned char* data = impl->bytes.data();
    if (stbtt_InitFont(&impl->info, data, 0) == 0) {
        fail(error, "stb_truetype could not parse the font");
        return std::nullopt;
    }
    if (stbtt_FindGlyphIndex(&impl->info, 'A') == 0) {
        fail(error, "font has no glyph for 'A'");
        return std::nullopt;
    }
    std::uint16_t units_per_em = 0;
    (void)read_u16(impl->bytes, static_cast<std::size_t>(impl->info.head) + 18, units_per_em);
    impl->units_per_em = units_per_em;
    stbtt_GetFontVMetrics(&impl->info, &impl->ascent, &impl->descent, &impl->line_gap);
    for (std::size_t slot = 0; slot < kBakedGlyphCount; ++slot) {
        const int glyph = stbtt_FindGlyphIndex(
            &impl->info, static_cast<int>(slot_code_point(static_cast<int>(slot))));
        impl->glyph[slot] = glyph;
        if (glyph != 0) {
            int advance = 0;
            int left_bearing = 0;
            stbtt_GetGlyphHMetrics(&impl->info, glyph, &advance, &left_bearing);
            impl->advance[slot] = advance;
        }
    }
    impl->cap_height =
        static_cast<int>(std::lround(kDefaultCapHeightEm * static_cast<float>(units_per_em)));
    const int h_glyph = impl->glyph[static_cast<std::size_t>(baked_glyph_slot(U'H'))];
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    if (h_glyph != 0 && stbtt_GetGlyphBox(&impl->info, h_glyph, &x0, &y0, &x1, &y1) != 0 &&
        y1 > 0) {
        impl->cap_height = y1;
    }
    return FontFace(std::move(impl));
}

std::optional<FontFace> FontFace::from_file(const std::filesystem::path& path,
                                            std::string* error) {
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec) {
        fail(error, "missing or unreadable file");
        return std::nullopt;
    }
    if (size > kMaxFontBytes) {
        fail(error, "file is larger than the 16 MiB font cap");
        return std::nullopt;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    std::ifstream file(path, std::ios::binary);
    if (!file || (size > 0 && !file.read(reinterpret_cast<char*>(bytes.data()),
                                         static_cast<std::streamsize>(size)))) {
        fail(error, "could not read the file");
        return std::nullopt;
    }
    return from_bytes(std::move(bytes), error);
}

int FontFace::units_per_em() const { return impl_->units_per_em; }
int FontFace::ascent() const { return impl_->ascent; }
int FontFace::descent() const { return impl_->descent; }
int FontFace::line_gap() const { return impl_->line_gap; }
int FontFace::cap_height_units() const { return impl_->cap_height; }

float FontFace::em_scale(float pixel_size) const {
    if (!finite_positive(pixel_size) || impl_->units_per_em <= 0) {
        return 0.0f;
    }
    return pixel_size / static_cast<float>(impl_->units_per_em);
}

bool FontFace::has(int slot) const {
    return glyph_index(slot) != 0;
}

int FontFace::glyph_index(int slot) const {
    if (slot < 0 || slot >= static_cast<int>(kBakedGlyphCount)) {
        return 0;
    }
    return impl_->glyph[static_cast<std::size_t>(slot)];
}

int FontFace::advance_units(int slot) const {
    if (!has(slot)) {
        return 0;
    }
    return impl_->advance[static_cast<std::size_t>(slot)];
}

int FontFace::kern_units(int slot_a, int slot_b) const {
    const int a = glyph_index(slot_a);
    const int b = glyph_index(slot_b);
    if (a == 0 || b == 0) {
        return 0;
    }
    return stbtt_GetGlyphKernAdvance(&impl_->info, a, b);
}

// ---------------------------------------------------------------------------------------------
// FontAtlas
// ---------------------------------------------------------------------------------------------

namespace {

struct PlaceholderBox {
    int width = 0;
    int height = 0;
    int stroke = 0;
};

[[nodiscard]] PlaceholderBox placeholder_box(const FontFace& face, float pixel_size) {
    PlaceholderBox box;
    const float cap_px = static_cast<float>(face.cap_height_units()) * face.em_scale(pixel_size);
    box.width = std::max(3, static_cast<int>(std::lround(kPlaceholderWidthEm * pixel_size)));
    box.height = std::max(3, static_cast<int>(std::lround(cap_px)));
    box.stroke = std::max(1, static_cast<int>(std::lround(kPlaceholderStrokeEm * pixel_size)));
    box.stroke = std::min(box.stroke, std::min(box.width, box.height) / 2);
    box.stroke = std::max(1, box.stroke);
    return box;
}

[[nodiscard]] int next_power_of_two(double value) {
    int result = 1;
    while (static_cast<double>(result) < value && result < (1 << 30)) {
        result <<= 1;
    }
    return result;
}

} // namespace

std::optional<FontAtlas> FontAtlas::bake(const FontFace& face, float pixel_size, int max_dim,
                                         std::string* error) {
    if (!std::isfinite(pixel_size) || pixel_size < kMinPixelSize || pixel_size > kMaxPixelSize) {
        fail(error, "pixel size out of range");
        return std::nullopt;
    }
    const int cap = std::clamp(max_dim, kMinAtlasDim, kMaxAtlasDim);
    const FontFace::Impl& font = *face.impl_;
    const float k_em = face.em_scale(pixel_size);
    const PlaceholderBox box = placeholder_box(face, pixel_size);

    for (const int over : {2, 1}) {
        // Estimate the packed area to size the first try.
        double area = static_cast<double>(box.width + 1) * static_cast<double>(box.height + 1);
        for (std::size_t slot = 0; slot < kBakedGlyphCount; ++slot) {
            const int glyph = font.glyph[slot];
            if (glyph == 0) {
                continue;
            }
            int x0 = 0;
            int y0 = 0;
            int x1 = 0;
            int y1 = 0;
            stbtt_GetGlyphBitmapBox(&font.info, glyph, k_em * static_cast<float>(over),
                                    k_em * static_cast<float>(over), &x0, &y0, &x1, &y1);
            area += static_cast<double>(x1 - x0 + over) * static_cast<double>(y1 - y0 + over);
        }
        int width = std::clamp(next_power_of_two(std::sqrt(area * 1.2)), kMinAtlasDim, cap);

        while (true) {
            std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) *
                                             static_cast<std::size_t>(cap));
            stbtt_pack_context context{};
            if (stbtt_PackBegin(&context, pixels.data(), width, cap, 0, 1, nullptr) == 0) {
                fail(error, "out of memory");
                return std::nullopt;
            }
            stbtt_PackSetOversampling(&context, static_cast<unsigned int>(over),
                                      static_cast<unsigned int>(over));
            stbtt_PackSetSkipMissingCodepoints(&context, 1);
            std::array<stbtt_packedchar, kBakedGlyphCount> chars{};
            std::array<stbtt_pack_range, kBakedRanges.size()> ranges{};
            for (std::size_t i = 0; i < kBakedRanges.size(); ++i) {
                ranges[i].font_size = STBTT_POINT_SIZE(pixel_size);
                ranges[i].first_unicode_codepoint_in_range =
                    static_cast<int>(kBakedRanges[i].first);
                ranges[i].array_of_unicode_codepoints = nullptr;
                ranges[i].num_chars = kBakedRanges[i].count;
                ranges[i].chardata_for_range =
                    chars.data() + static_cast<std::size_t>(kBakedRanges[i].first_slot);
            }
            // The return value is 0 whenever a missing code point was skipped,
            // so success is checked per glyph below instead.
            (void)stbtt_PackFontRanges(&context, font.bytes.data(), 0, ranges.data(),
                                       static_cast<int>(ranges.size()));
            stbrp_rect rect{};
            rect.w = static_cast<stbrp_coord>(box.width + 1); // + padding
            rect.h = static_cast<stbrp_coord>(box.height + 1);
            stbtt_PackFontRangesPackRects(&context, &rect, 1);
            stbtt_PackEnd(&context);

            // A packed glyph sits at x0 >= padding (1); every char starts zeroed.
            bool all_packed = rect.was_packed != 0;
            int used_height = 0;
            for (std::size_t slot = 0; all_packed && slot < kBakedGlyphCount; ++slot) {
                if (font.glyph[slot] == 0) {
                    continue;
                }
                if (chars[slot].x0 < 1) {
                    all_packed = false;
                }
                used_height = std::max(used_height, static_cast<int>(chars[slot].y1));
            }
            if (all_packed) {
                const int box_x = rect.x + 1;
                const int box_y = rect.y + 1;
                used_height = std::max(used_height, box_y + box.height);
                const int height = std::min(cap, (used_height + 1 + 3) / 4 * 4);

                // Hollow placeholder box, 1:1 texels (not oversampled).
                for (int y = 0; y < box.height; ++y) {
                    for (int x = 0; x < box.width; ++x) {
                        const bool edge = x < box.stroke || y < box.stroke ||
                                          x >= box.width - box.stroke ||
                                          y >= box.height - box.stroke;
                        if (edge) {
                            pixels[static_cast<std::size_t>(box_y + y) *
                                       static_cast<std::size_t>(width) +
                                   static_cast<std::size_t>(box_x + x)] = 255;
                        }
                    }
                }
                pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));

                FontAtlas atlas;
                atlas.pixel_size = pixel_size;
                atlas.width = width;
                atlas.height = height;
                atlas.oversample = over;
                atlas.coverage = std::move(pixels);
                for (std::size_t slot = 0; slot < kBakedGlyphCount; ++slot) {
                    const int glyph = font.glyph[slot];
                    if (glyph == 0) {
                        continue; // absent: present stays false
                    }
                    AtlasGlyph& out = atlas.glyphs[slot];
                    out.present = true;
                    if (stbtt_IsGlyphEmpty(&font.info, glyph) != 0) {
                        continue; // no ink (space): zero-area quad
                    }
                    float pen_x = 0.0f;
                    float pen_y = 0.0f;
                    stbtt_aligned_quad quad{};
                    stbtt_GetPackedQuad(chars.data(), width, height, static_cast<int>(slot),
                                        &pen_x, &pen_y, &quad, 0);
                    out.x0 = quad.x0;
                    out.y0 = quad.y0;
                    out.x1 = quad.x1;
                    out.y1 = quad.y1;
                    out.uv = UVRect{quad.s0, quad.t0, quad.s1, quad.t1};
                }
                AtlasGlyph& placeholder = atlas.placeholder;
                placeholder.present = true;
                placeholder.x0 = kPlaceholderLeftEm * pixel_size;
                placeholder.x1 = placeholder.x0 + static_cast<float>(box.width);
                placeholder.y0 = -static_cast<float>(box.height);
                placeholder.y1 = 0.0f;
                placeholder.uv = UVRect{
                    static_cast<float>(box_x) / static_cast<float>(width),
                    static_cast<float>(box_y) / static_cast<float>(height),
                    static_cast<float>(box_x + box.width) / static_cast<float>(width),
                    static_cast<float>(box_y + box.height) / static_cast<float>(height)};
                return atlas;
            }
            if (width >= cap) {
                break;
            }
            width = std::min(width * 2, cap);
        }
    }
    fail(error, "glyphs do not fit in a " + std::to_string(cap) + "x" + std::to_string(cap) +
                    " atlas");
    return std::nullopt;
}

bool FontAtlas::upload() {
    if (width <= 0 || height <= 0 ||
        coverage.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        return false;
    }
    const std::vector<std::uint8_t> rgba = coverage_to_white_rgba(coverage);
    texture = Texture::from_rgba(width, height, rgba.data(), /*mipmaps*/ false,
                                 Texture::Wrap::Clamp, Texture::Filter::Linear);
    if (!texture.valid()) {
        return false;
    }
    coverage.clear();
    coverage.shrink_to_fit();
    return true;
}

// ---------------------------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------------------------

float measure_text(const FontFace& face, std::string_view text, float pixel_size,
                   float tracking) {
    if (text.empty() || !finite_positive(pixel_size)) {
        return 0.0f;
    }
    const float track = sanitize(tracking);
    float pen = 0.0f;
    walk_glyphs(face, text, pixel_size, face.em_scale(pixel_size),
                [&](char32_t, int, float kern, float advance) { pen += kern + advance + track; });
    return pen;
}

void for_each_text_quad(const FontFace& face, const FontAtlas& atlas, std::string_view text,
                        float x, float y, const TextLayout& layout,
                        const std::function<void(const GlyphQuad&)>& emit) {
    const float pixel_size = layout.pixel_size;
    if (text.empty() || !std::isfinite(x) || !std::isfinite(y) || !finite_positive(pixel_size)) {
        return;
    }
    const float k_em = face.em_scale(pixel_size);
    const float tracking = sanitize(layout.tracking);
    const float shear = sanitize(layout.shear);
    const float glyph_scale =
        finite_positive(atlas.pixel_size) ? pixel_size / atlas.pixel_size : 1.0f;
    const float width = measure_text(face, text, pixel_size, tracking);
    const float start = aligned_start(x, width, layout.align);
    const float baseline = y + static_cast<float>(face.ascent()) * k_em;
    const float shadow = sanitize(layout.shadow_offset);

    const auto pass = [&](Color color, float pass_baseline) {
        float pen = start;
        walk_glyphs(face, text, pixel_size, k_em,
                    [&](char32_t, int slot, float kern, float advance) {
                        pen += kern;
                        const AtlasGlyph* glyph = &atlas.placeholder;
                        if (slot >= 0 && atlas.glyphs[static_cast<std::size_t>(slot)].present) {
                            glyph = &atlas.glyphs[static_cast<std::size_t>(slot)];
                        }
                        if (glyph->x1 > glyph->x0 && glyph->y1 > glyph->y0) {
                            const float left = pen + glyph->x0 * glyph_scale;
                            const float right = pen + glyph->x1 * glyph_scale;
                            const float top = pass_baseline + glyph->y0 * glyph_scale;
                            const float bottom = pass_baseline + glyph->y1 * glyph_scale;
                            const float top_shift = shear * (pass_baseline - top);
                            const float bottom_shift = shear * (pass_baseline - bottom);
                            GlyphQuad quad;
                            quad.corners = {Vec2{left + top_shift, top},
                                            Vec2{right + top_shift, top},
                                            Vec2{right + bottom_shift, bottom},
                                            Vec2{left + bottom_shift, bottom}};
                            quad.uv = glyph->uv;
                            quad.color = color;
                            const bool finite = std::all_of(
                                quad.corners.begin(), quad.corners.end(), [](const Vec2& v) {
                                    return std::isfinite(v.x) && std::isfinite(v.y);
                                });
                            if (finite) {
                                emit(quad);
                            }
                        }
                        pen += advance + tracking;
                    });
    };
    if (shadow > 0.0f) {
        pass(shadow_color(layout.color), baseline + shadow);
    }
    pass(layout.color, baseline);
}

// ---------------------------------------------------------------------------------------------
// TextRenderer
// ---------------------------------------------------------------------------------------------

TextRenderer::TextRenderer() {
    atlases_.reserve(kMaxAtlases);
}

TextRenderer::~TextRenderer() = default;

void TextRenderer::shutdown() {
    atlases_.clear();
    for (std::optional<FontFace>& face : faces_) {
        face.reset();
    }
    baked_width_ = -1;
    baked_height_ = -1;
    sized_ = false;
    scale_ = 1.0f;
    warned_headless_ = false;
    warned_atlas_cap_ = false;
    warned_fallback_.fill(false);
}

bool TextRenderer::load() {
    std::array<std::filesystem::path, theme::kFontCount> paths;
    for (std::size_t i = 0; i < theme::kFontCount; ++i) {
        const std::filesystem::path relative(theme::kFontFiles[i]);
        const std::filesystem::path resolved =
            resolve_first_existing({relative, default_executable_dir() / relative});
        paths[i] = resolved.empty() ? relative : resolved;
    }
    return load_paths(paths);
}

bool TextRenderer::load(const std::filesystem::path& root) {
    std::array<std::filesystem::path, theme::kFontCount> paths;
    for (std::size_t i = 0; i < theme::kFontCount; ++i) {
        paths[i] = root / theme::kFontFiles[i];
    }
    return load_paths(paths);
}

bool TextRenderer::load_paths(const std::array<std::filesystem::path, theme::kFontCount>& paths) {
    shutdown();
    std::size_t loaded = 0;
    for (std::size_t i = 0; i < theme::kFontCount; ++i) {
        std::string error;
        faces_[i] = FontFace::from_file(paths[i], &error);
        if (faces_[i]) {
            ++loaded;
        } else {
            std::cerr << "[TextRenderer] Font unavailable: " << paths[i].string() << " (" << error
                      << "); " << font_name(static_cast<theme::Font>(i))
                      << " text uses the bitmap fallback\n";
        }
    }
    std::cout << "[TextRenderer] Loaded " << loaded << "/" << theme::kFontCount << " fonts\n";
    return loaded > 0;
}

void TextRenderer::set_window_size(int window_width, int window_height) {
    if (sized_ && window_width == baked_width_ && window_height == baked_height_) {
        return;
    }
    baked_width_ = window_width;
    baked_height_ = window_height;
    const float scale = text_layout_scale(window_width, window_height);
    // Atlases depend only on the scale: a resize that keeps it (e.g. widening a
    // height-limited 21:9 window) keeps them.
    if (sized_ && scale == scale_) {
        return;
    }
    sized_ = true;
    scale_ = scale;
    atlases_.clear();
    if (!gl_present()) {
        if (!warned_headless_) {
            warned_headless_ = true;
            std::cerr << "[TextRenderer] No GL context available; text is measured but not "
                         "drawn (no atlases baked)\n";
        }
        return;
    }
    ensure_max_texture_size();

    const auto started = std::chrono::steady_clock::now();
    std::size_t baked = 0;
    std::size_t bytes = 0;
    for (const theme::TextStyle& style : theme::text::kAllStyles) {
        if (!face(style.font)) {
            continue;
        }
        const bool known = std::any_of(atlases_.begin(), atlases_.end(), [&](const AtlasSlot& s) {
            return s.font == style.font && s.size_px == style.size_px;
        });
        if (known || atlases_.size() >= kMaxAtlases) {
            continue;
        }
        const AtlasSlot& slot = bake_slot(style.font, style.size_px);
        if (slot.atlas) {
            ++baked;
            bytes += static_cast<std::size_t>(slot.atlas->width) *
                     static_cast<std::size_t>(slot.atlas->height) * 4u;
        }
    }
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - started)
                          .count();
    const std::ios::fmtflags saved_flags = std::cout.flags();
    const std::streamsize saved_precision = std::cout.precision();
    std::cout << "[TextRenderer] Baked " << baked << " atlases for " << window_width << "x"
              << window_height << " (s=" << std::fixed << std::setprecision(2) << scale_ << ", "
              << std::setprecision(1)
              << static_cast<double>(bytes) / (1024.0 * 1024.0) << " MB, " << ms << " ms)\n";
    std::cout.flags(saved_flags);
    std::cout.precision(saved_precision);
}

void TextRenderer::ensure_max_texture_size() {
    if (max_texture_size_ != 0) {
        return;
    }
    GLint gl_max = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gl_max);
    // GL 3.3 guarantees at least 1024.
    max_texture_size_ = gl_max > 0 ? std::min(kMaxAtlasDim, static_cast<int>(gl_max)) : 1024;
}

const FontFace* TextRenderer::face(theme::Font font) const {
    const std::size_t index = font_slot(font);
    if (index >= faces_.size() || !faces_[index]) {
        return nullptr;
    }
    return &*faces_[index];
}

TextRenderer::AtlasSlot& TextRenderer::bake_slot(theme::Font font, float size_px) {
    AtlasSlot slot;
    slot.font = font;
    slot.size_px = size_px;
    const float pixel_size = size_px * scale_;
    std::string error;
    if (const FontFace* source = face(font)) {
        slot.atlas = FontAtlas::bake(*source, pixel_size, max_texture_size_, &error);
        if (slot.atlas && !slot.atlas->upload()) {
            error = "texture upload failed";
            slot.atlas.reset();
        }
    } else {
        error = "font not loaded";
    }
    if (!slot.atlas) {
        std::cerr << "[FontAtlas] Could not bake " << font_name(font) << " " << size_px
                  << "px at " << pixel_size << "px (" << error
                  << "); that style uses the bitmap fallback\n";
    }
    atlases_.push_back(std::move(slot));
    return atlases_.back();
}

const FontAtlas* TextRenderer::atlas_for(theme::Font font, float size_px, bool lazy) {
    for (const AtlasSlot& slot : atlases_) {
        if (slot.font == font && slot.size_px == size_px) {
            return slot.atlas ? &*slot.atlas : nullptr;
        }
    }
    if (!lazy || !gl_present() || face(font) == nullptr) {
        return nullptr;
    }
    ensure_max_texture_size();
    if (atlases_.size() >= kMaxAtlases) {
        if (!warned_atlas_cap_) {
            warned_atlas_cap_ = true;
            std::cerr << "[TextRenderer] Atlas cap (" << kMaxAtlases << ") reached; "
                      << font_name(font) << " " << size_px
                      << "px and later new sizes use the bitmap fallback\n";
        }
        return nullptr;
    }
    std::cerr << "[TextRenderer] " << font_name(font) << " " << size_px
              << "px is not in theme::text::kAllStyles; baking it lazily\n";
    const AtlasSlot& slot = bake_slot(font, size_px);
    return slot.atlas ? &*slot.atlas : nullptr;
}

float TextRenderer::measure(std::string_view text, const theme::TextStyle& style) const {
    const float pixel_size = style.size_px * scale_;
    const float tracking = style.tracking_px * scale_;
    if (const FontFace* source = face(style.font)) {
        return measure_text(*source, text, pixel_size, tracking);
    }
    if (!finite_positive(pixel_size)) {
        return 0.0f;
    }
    return bitmap_measure(text, pixel_size, sanitize(tracking));
}

float TextRenderer::ascent(const theme::TextStyle& style) const {
    // The same baseline the bitmap fallback draws on, with or without a face.
    return bitmap_fallback_baseline(face(style.font), style.size_px * scale_);
}

float TextRenderer::line_height(const theme::TextStyle& style) const {
    const float pixel_size = style.size_px * scale_;
    if (const FontFace* source = face(style.font)) {
        return static_cast<float>(source->ascent() - source->descent() + source->line_gap()) *
               source->em_scale(pixel_size);
    }
    return finite_positive(pixel_size) ? kFallbackLineEm * pixel_size : 0.0f;
}

std::string TextRenderer::truncate(std::string_view text, const theme::TextStyle& style,
                                   float max_width) const {
    return truncate_to_width(text, max_width,
                             [&](std::string_view candidate) { return measure(candidate, style); });
}

bool TextRenderer::covers_text(std::string_view text, theme::Font font) const {
    const FontFace* source = face(font);
    if (source == nullptr) {
        return font_covers_text(text);
    }
    std::size_t pos = 0;
    while (pos < text.size()) {
        const char32_t cp = next_code_point(text, pos);
        if (is_zero_width(cp)) {
            continue;
        }
        if (!source->has(baked_glyph_slot(cp))) {
            return false;
        }
    }
    return true;
}

bool TextRenderer::font_available(theme::Font font) const {
    return face(font) != nullptr;
}

std::size_t TextRenderer::atlas_count() const {
    return static_cast<std::size_t>(std::count_if(
        atlases_.begin(), atlases_.end(), [](const AtlasSlot& slot) { return slot.atlas.has_value(); }));
}

void TextRenderer::draw(GlQuadRenderer& renderer, std::string_view text, float x, float y,
                        const theme::TextStyle& style, TextAlign align, float extra_shear) {
    const std::size_t index = font_slot(style.font);
    if (index >= faces_.size()) {
        return;
    }
    const TextLayout layout = resolve_text_layout(style, scale_, align, extra_shear);
    const FontFace* source = face(style.font);
    if (source == nullptr) {
        // Logged before any renderer check so the log-once rule holds headless.
        if (!warned_fallback_[index]) {
            warned_fallback_[index] = true;
            std::cerr << "[TextRenderer] " << font_name(style.font)
                      << " unavailable; using the bitmap fallback\n";
        }
        draw_bitmap_fallback(renderer, nullptr, text, x, y, layout);
        return;
    }
    if (!gl_present() || !renderer.is_initialized()) {
        return; // headless: measuring only
    }
    const FontAtlas* atlas = atlas_for(style.font, style.size_px, /*lazy*/ true);
    if (atlas == nullptr || !atlas->texture.valid()) {
        if (!warned_fallback_[index]) {
            warned_fallback_[index] = true;
            std::cerr << "[TextRenderer] " << font_name(style.font) << " " << style.size_px
                      << "px atlas unavailable; using the bitmap fallback\n";
        }
        draw_bitmap_fallback(renderer, source, text, x, y, layout);
        return;
    }
    const Texture& texture = atlas->texture;
    for_each_text_quad(*source, *atlas, text, x, y, layout, [&](const GlyphQuad& quad) {
        renderer.draw_quad_points(quad.corners, texture, quad.uv,
                                  {quad.color, quad.color, quad.color, quad.color});
    });
}

void TextRenderer::draw_bitmap_fallback(GlQuadRenderer& renderer, const FontFace* source,
                                        std::string_view text, float x, float y,
                                        const TextLayout& layout) const {
    const float pixel_size = layout.pixel_size;
    if (!renderer.is_initialized() || text.empty() || !std::isfinite(x) || !std::isfinite(y) ||
        !finite_positive(pixel_size)) {
        return;
    }
    const float pixel = kFallbackPixelPerEm * pixel_size;
    const float tracking = sanitize(layout.tracking);
    // With a face (atlas bake failed) the cells follow the face's pen so the
    // draw matches measure(); without one, 6 * pixel + tracking per cell.
    const float width = source != nullptr ? measure_text(*source, text, pixel_size, tracking)
                                          : bitmap_measure(text, pixel_size, tracking);
    const float start = aligned_start(x, width, layout.align);
    // Rows end on the baseline ascent() reports (the face's when there is one).
    const float top = y + bitmap_fallback_baseline(source, pixel_size) - 7.0f * pixel;

    const auto draw_cell = [&](char32_t cp, float pen, float row_top, Color color) {
        const std::uint8_t* rows = glyph_rows(cp);
        if (rows == nullptr) {
            return;
        }
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if ((rows[row] >> (4 - col)) & 1u) {
                    renderer.draw_quad(Rect{pen + static_cast<float>(col) * pixel,
                                            row_top + static_cast<float>(row) * pixel, pixel,
                                            pixel},
                                       color);
                }
            }
        }
    };
    const auto pass = [&](Color color, float row_top) {
        float pen = start;
        if (source != nullptr) {
            walk_glyphs(*source, text, pixel_size, source->em_scale(pixel_size),
                        [&](char32_t cp, int, float kern, float advance) {
                            pen += kern;
                            draw_cell(cp, pen, row_top, color);
                            pen += advance + tracking;
                        });
            return;
        }
        std::size_t pos = 0;
        while (pos < text.size()) {
            const char32_t cp = next_code_point(text, pos);
            if (is_zero_width(cp)) {
                continue;
            }
            draw_cell(cp, pen, row_top, color);
            pen += 6.0f * pixel + tracking;
        }
    };
    const float shadow = sanitize(layout.shadow_offset);
    if (shadow > 0.0f) {
        pass(shadow_color(layout.color), top + shadow);
    }
    pass(layout.color, top);
}

} // namespace blaze4k
