#include "render/theme_textures.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <glad/glad.h>
#include <nlohmann/json.hpp>

#include "data/data_paths.hpp"
#include "render/gl_quad_renderer.hpp"
#include "render/theme.hpp"

namespace blaze4k {

namespace {

using json = nlohmann::json;

constexpr std::uintmax_t kMaxManifestBytes = 1u << 20; // 1 MiB cap for untrusted config
constexpr std::size_t kMaxTextures = 256;
constexpr std::size_t kMaxFonts = 8;
constexpr std::size_t kMaxGlyphKeys = 32;
constexpr std::size_t kMaxNameLength = 128;
constexpr int kMaxPx = 4096; // matches the Texture untrusted-image dimension cap
constexpr float kMaxTextureScale = 8.0f;
constexpr float kDefaultTextureScale = 2.0f;
constexpr float kFallbackAlphaScale = 0.25f;

struct ParseError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

[[nodiscard]] const json* member(const json& object, const char* key) {
    const auto it = object.find(key);
    return it == object.end() ? nullptr : &*it;
}

[[nodiscard]] int read_int(const json& value, int lo, int hi, const std::string& what) {
    if (!value.is_number_integer()) {
        throw ParseError(what + " is not an integer");
    }
    long long number = 0;
    if (value.is_number_unsigned()) {
        const auto unsigned_number = value.get<std::uint64_t>();
        if (unsigned_number > static_cast<std::uint64_t>(hi)) {
            throw ParseError(what + " is out of range");
        }
        number = static_cast<long long>(unsigned_number);
    } else {
        number = value.get<std::int64_t>();
    }
    if (number < lo || number > hi) {
        throw ParseError(what + " is out of range");
    }
    return static_cast<int>(number);
}

[[nodiscard]] float read_float(const json& value, float lo, float hi, const std::string& what) {
    if (!value.is_number()) {
        throw ParseError(what + " is not a number");
    }
    const double number = value.get<double>();
    if (!std::isfinite(number) || number < lo || number > hi) {
        throw ParseError(what + " is out of range");
    }
    return static_cast<float>(number);
}

[[nodiscard]] const json& require(const json& object, const char* key) {
    const json* value = member(object, key);
    if (value == nullptr) {
        throw ParseError(std::string("missing ") + key);
    }
    return *value;
}

// [x, y, w, h] with every component in [0, 4096] and w, h > 0.
[[nodiscard]] PxRect read_rect(const json& value, const char* what) {
    if (!value.is_array() || value.size() != 4) {
        throw ParseError(std::string(what) + " is not [x, y, w, h]");
    }
    PxRect rect;
    rect.x = read_int(value[0], 0, kMaxPx, what);
    rect.y = read_int(value[1], 0, kMaxPx, what);
    rect.w = read_int(value[2], 1, kMaxPx, what);
    rect.h = read_int(value[3], 1, kMaxPx, what);
    return rect;
}

[[nodiscard]] bool rect_inside(const PxRect& rect, int width, int height) {
    return rect.x + rect.w <= width && rect.y + rect.h <= height;
}

// A plain `*.png` file name: no directories, no traversal, no drive colon.
[[nodiscard]] bool plain_png_name(std::string_view file) {
    if (file.size() <= 4 || file.size() > 255) {
        return false;
    }
    if (file.front() == '.' || file.find("..") != std::string_view::npos ||
        file.find_first_of("/\\:") != std::string_view::npos) {
        return false;
    }
    for (const char c : file) {
        if (static_cast<unsigned char>(c) < 0x20) {
            return false;
        }
    }
    return file.ends_with(".png");
}

[[nodiscard]] std::string read_file_name(const json& object) {
    const json& file = require(object, "file");
    if (!file.is_string()) {
        throw ParseError("file is not a string");
    }
    std::string name = file.get<std::string>();
    if (!plain_png_name(name)) {
        throw ParseError("file '" + name + "' is not a plain .png file name");
    }
    return name;
}

[[nodiscard]] ThemeKind read_kind(const json& object) {
    const json& kind = require(object, "kind");
    if (!kind.is_string()) {
        throw ParseError("kind is not a string");
    }
    const std::string& text = kind.get_ref<const std::string&>();
    static constexpr std::pair<std::string_view, ThemeKind> kKinds[] = {
        {"fullscreen", ThemeKind::Fullscreen}, {"sprite", ThemeKind::Sprite},
        {"stretch_x", ThemeKind::StretchX},    {"stretch", ThemeKind::Stretch},
        {"slice3", ThemeKind::Slice3},         {"slice9", ThemeKind::Slice9},
        {"frame", ThemeKind::Frame},           {"tile", ThemeKind::Tile},
    };
    for (const auto& [name, value] : kKinds) {
        if (text == name) {
            return value;
        }
    }
    throw ParseError("unknown kind '" + text + "'");
}

[[nodiscard]] ThemeEntry parse_entry(const json& value) {
    if (!value.is_object()) {
        throw ParseError("entry is not an object");
    }
    ThemeEntry entry;
    entry.file = read_file_name(value);
    entry.kind = read_kind(value);

    const json& size = require(value, "size_px");
    if (!size.is_array() || size.size() != 2) {
        throw ParseError("size_px is not [w, h]");
    }
    entry.width = read_int(size[0], 1, kMaxPx, "size_px");
    entry.height = read_int(size[1], 1, kMaxPx, "size_px");

    if (const json* content = member(value, "content_px")) {
        entry.content = read_rect(*content, "content_px");
        if (!rect_inside(entry.content, entry.width, entry.height)) {
            throw ParseError("content_px lies outside the image");
        }
    } else {
        entry.content = PxRect{0, 0, entry.width, entry.height};
    }

    switch (entry.kind) {
    case ThemeKind::Slice3: {
        const json& slice = require(value, "slice3_px");
        if (!slice.is_object()) {
            throw ParseError("slice3_px is not an object");
        }
        Slice3Px px;
        px.left = read_int(require(slice, "left"), 0, kMaxPx, "slice3_px.left");
        px.right = read_int(require(slice, "right"), 0, kMaxPx, "slice3_px.right");
        if (px.left + px.right >= entry.width) {
            throw ParseError("slice3_px caps are not narrower than the image");
        }
        entry.slice3 = px;
        break;
    }
    case ThemeKind::Slice9: {
        const json& slice = require(value, "slice9_px");
        if (!slice.is_object()) {
            throw ParseError("slice9_px is not an object");
        }
        Slice9Px px;
        px.top = read_int(require(slice, "top"), 0, kMaxPx, "slice9_px.top");
        px.right = read_int(require(slice, "right"), 0, kMaxPx, "slice9_px.right");
        px.bottom = read_int(require(slice, "bottom"), 0, kMaxPx, "slice9_px.bottom");
        px.left = read_int(require(slice, "left"), 0, kMaxPx, "slice9_px.left");
        if (px.left + px.right >= entry.width || px.top + px.bottom >= entry.height) {
            throw ParseError("slice9_px borders are not smaller than the image");
        }
        entry.slice9 = px;
        break;
    }
    case ThemeKind::Frame: {
        const PxRect hole = read_rect(require(value, "hole_px"), "hole_px");
        if (!rect_inside(hole, entry.width, entry.height)) {
            throw ParseError("hole_px lies outside the image");
        }
        entry.hole = hole;
        break;
    }
    case ThemeKind::Tile: {
        const json* scale = member(value, "scale");
        entry.screen_pixel_tile =
            scale != nullptr && scale->is_string() &&
            scale->get_ref<const std::string&>().starts_with("1x");
        break;
    }
    default:
        break;
    }
    return entry;
}

[[nodiscard]] DigitGlyph parse_glyph(const json& value) {
    if (!value.is_object()) {
        throw ParseError("glyph is not an object");
    }
    DigitGlyph glyph;
    glyph.src.x = read_int(require(value, "x"), 0, kMaxPx, "glyph x");
    glyph.src.y = read_int(require(value, "y"), 0, kMaxPx, "glyph y");
    glyph.src.w = read_int(require(value, "w"), 1, kMaxPx, "glyph w");
    glyph.src.h = read_int(require(value, "h"), 1, kMaxPx, "glyph h");
    glyph.origin_x = read_float(require(value, "origin_x"), -static_cast<float>(kMaxPx),
                                static_cast<float>(kMaxPx), "glyph origin_x");
    glyph.advance =
        read_float(require(value, "advance"), 0.0f, static_cast<float>(kMaxPx), "glyph advance");
    glyph.present = true;
    return glyph;
}

void warn(std::vector<std::string>* warnings, std::string message) {
    if (warnings != nullptr) {
        warnings->push_back(std::move(message));
    }
}

[[nodiscard]] DigitFont parse_font(const std::string& name, const json& value,
                                   std::vector<std::string>* warnings) {
    if (!value.is_object()) {
        throw ParseError("font is not an object");
    }
    DigitFont font;
    font.file = read_file_name(value);
    const json& glyphs = require(value, "glyphs");
    if (!glyphs.is_object()) {
        throw ParseError("glyphs is not an object");
    }
    std::size_t seen = 0;
    bool any = false;
    for (auto it = glyphs.begin(); it != glyphs.end(); ++it) {
        if (++seen > kMaxGlyphKeys) {
            warn(warnings, "font '" + name + "': more than 32 glyph keys; the rest are ignored");
            break;
        }
        const std::string& key = it.key();
        const int index = key.size() == 1 ? digit_glyph_index(key[0]) : -1;
        if (index < 0) {
            warn(warnings, "font '" + name + "': unsupported glyph key '" + key + "' ignored");
            continue;
        }
        try {
            font.glyphs[static_cast<std::size_t>(index)] = parse_glyph(it.value());
            any = true;
        } catch (const std::exception& ex) {
            warn(warnings, "font '" + name + "': glyph '" + key + "' skipped: " + ex.what());
        }
    }
    if (!any) {
        throw ParseError("no valid glyphs");
    }
    return font;
}

[[nodiscard]] bool finite_positive(float value) {
    return std::isfinite(value) && value > 0.0f;
}

[[nodiscard]] bool finite_rect(const Rect& rect) {
    return std::isfinite(rect.x) && std::isfinite(rect.y) && std::isfinite(rect.w) &&
           std::isfinite(rect.h);
}

[[nodiscard]] bool drawable(const Rect& rect) {
    return finite_rect(rect) && rect.w > 0.0f && rect.h > 0.0f;
}

[[nodiscard]] bool has_content(const ThemeEntry& entry) {
    return entry.width > 0 && entry.height > 0 && entry.content.w > 0 && entry.content.h > 0;
}

// Splits `total` into two borders (a, b) and a middle, shrinking both borders
// proportionally when they do not fit.
struct AxisSplit {
    float a;
    float mid;
    float b;
};
[[nodiscard]] AxisSplit split_axis(float a, float b, float total) {
    if (a + b > total) {
        const float f = (a + b) > 0.0f ? total / (a + b) : 0.0f;
        a *= f;
        b *= f;
    }
    return AxisSplit{a, std::max(0.0f, total - a - b), b};
}

void draw_flat(GlQuadRenderer& renderer, const Rect& rect, Color color) {
    if (drawable(rect)) {
        renderer.draw_quad(rect, color);
    }
}

void draw_textured(GlQuadRenderer& renderer, const Rect& rect, const Texture& texture,
                   const UVRect& uv, Color tint) {
    if (drawable(rect)) {
        renderer.draw_textured_quad(rect, texture, uv, tint);
    }
}

// Colour used for an unknown name's flat fallback (no kind is known).
[[nodiscard]] Color unknown_fallback_color(Color tint) {
    return theme_fallback_color(ThemeKind::Sprite, tint);
}

// Shared stretch path: the content box fills `content_rect` per axis.
void draw_stretched(GlQuadRenderer& renderer, const ThemeEntry& entry, const Texture& texture,
                    const Rect& content_rect, Color tint) {
    if (!drawable(content_rect)) {
        return;
    }
    if (texture.valid()) {
        draw_textured(renderer, content_to_image_rect(entry, content_rect), texture, UVRect{},
                      tint);
    } else {
        draw_flat(renderer, content_rect, theme_fallback_color(entry.kind, tint));
    }
}

} // namespace

int digit_glyph_index(char c) {
    const std::size_t index = kDigitChars.find(c);
    return index == std::string_view::npos ? -1 : static_cast<int>(index);
}

ThemeManifest parse_theme_manifest(std::string_view json_text, std::vector<std::string>* warnings) {
    ThemeManifest manifest;
    try {
        const json document = json::parse(json_text.begin(), json_text.end(), nullptr, false);
        if (document.is_discarded() || !document.is_object()) {
            warn(warnings, "manifest is not a JSON object");
            return manifest;
        }

        if (const json* scale = member(document, "texture_scale")) {
            try {
                const float value = read_float(*scale, 0.0f, kMaxTextureScale, "texture_scale");
                if (value <= 0.0f) {
                    throw ParseError("texture_scale is out of range");
                }
                manifest.texture_scale = value;
            } catch (const std::exception& ex) {
                warn(warnings, std::string(ex.what()) + "; using 2");
                manifest.texture_scale = kDefaultTextureScale;
            }
        }

        if (const json* textures = member(document, "textures")) {
            if (!textures->is_object()) {
                warn(warnings, "textures is not an object; no textures loaded");
            } else {
                for (auto it = textures->begin(); it != textures->end(); ++it) {
                    const std::string& name = it.key();
                    if (manifest.textures.size() >= kMaxTextures) {
                        warn(warnings, "more than 256 textures; the rest are ignored");
                        break;
                    }
                    if (name.empty() || name.size() > kMaxNameLength) {
                        warn(warnings, "texture with an empty or overlong name skipped");
                        continue;
                    }
                    try {
                        manifest.textures.emplace(name, parse_entry(it.value()));
                    } catch (const std::exception& ex) {
                        warn(warnings, "texture '" + name + "' skipped: " + ex.what());
                    }
                }
            }
        }

        if (const json* fonts = member(document, "bitmap_fonts")) {
            if (!fonts->is_object()) {
                warn(warnings, "bitmap_fonts is not an object; no digit fonts loaded");
            } else {
                for (auto it = fonts->begin(); it != fonts->end(); ++it) {
                    const std::string& name = it.key();
                    if (manifest.fonts.size() >= kMaxFonts) {
                        warn(warnings, "more than 8 bitmap fonts; the rest are ignored");
                        break;
                    }
                    if (name.empty() || name.size() > kMaxNameLength) {
                        warn(warnings, "bitmap font with an empty or overlong name skipped");
                        continue;
                    }
                    try {
                        manifest.fonts.emplace(name, parse_font(name, it.value(), warnings));
                    } catch (const std::exception& ex) {
                        warn(warnings, "bitmap font '" + name + "' skipped: " + ex.what());
                    }
                }
            }
        }
    } catch (const std::exception& ex) {
        warn(warnings, std::string("manifest parse failed: ") + ex.what());
        return ThemeManifest{};
    }
    return manifest;
}

Rect sprite_content_rect(const ThemeEntry& entry, Vec2 content_pos, float k) {
    if (!finite_positive(k) || !std::isfinite(content_pos.x) || !std::isfinite(content_pos.y) ||
        !has_content(entry)) {
        return Rect{};
    }
    return Rect{content_pos.x, content_pos.y, static_cast<float>(entry.content.w) * k,
                static_cast<float>(entry.content.h) * k};
}

Rect content_to_image_rect(const ThemeEntry& entry, const Rect& content_rect) {
    if (!drawable(content_rect) || !has_content(entry)) {
        return Rect{};
    }
    const float kx = content_rect.w / static_cast<float>(entry.content.w);
    const float ky = content_rect.h / static_cast<float>(entry.content.h);
    return Rect{content_rect.x - static_cast<float>(entry.content.x) * kx,
                content_rect.y - static_cast<float>(entry.content.y) * ky,
                static_cast<float>(entry.width) * kx, static_cast<float>(entry.height) * ky};
}

std::array<ThemePiece, 3> slice3_pieces(const ThemeEntry& entry, const Rect& content_rect) {
    std::array<ThemePiece, 3> pieces{};
    if (!entry.slice3 || !has_content(entry) || !finite_rect(content_rect) ||
        content_rect.h <= 0.0f || content_rect.w < 0.0f) {
        return pieces;
    }
    const float iw = static_cast<float>(entry.width);
    const float ih = static_cast<float>(entry.height);
    const float k = content_rect.h / static_cast<float>(entry.content.h);
    const Rect image{content_rect.x - static_cast<float>(entry.content.x) * k,
                     content_rect.y - static_cast<float>(entry.content.y) * k,
                     content_rect.w + (iw - static_cast<float>(entry.content.w)) * k, ih * k};
    if (!drawable(image)) {
        return pieces;
    }
    const AxisSplit x = split_axis(static_cast<float>(entry.slice3->left) * k,
                                   static_cast<float>(entry.slice3->right) * k, image.w);
    const float u_left = static_cast<float>(entry.slice3->left) / iw;
    const float u_right = (iw - static_cast<float>(entry.slice3->right)) / iw;
    pieces[0] = ThemePiece{Rect{image.x, image.y, x.a, image.h}, UVRect{0.0f, 0.0f, u_left, 1.0f}};
    pieces[1] = ThemePiece{Rect{image.x + x.a, image.y, x.mid, image.h},
                           UVRect{u_left, 0.0f, u_right, 1.0f}};
    pieces[2] = ThemePiece{Rect{image.x + x.a + x.mid, image.y, x.b, image.h},
                           UVRect{u_right, 0.0f, 1.0f, 1.0f}};
    return pieces;
}

std::array<ThemePiece, 9> slice9_pieces(const ThemeEntry& entry, const Rect& content_rect,
                                        float k) {
    std::array<ThemePiece, 9> pieces{};
    if (!entry.slice9 || !has_content(entry) || !finite_positive(k) ||
        !finite_rect(content_rect) || content_rect.w < 0.0f || content_rect.h < 0.0f) {
        return pieces;
    }
    const float iw = static_cast<float>(entry.width);
    const float ih = static_cast<float>(entry.height);
    const Rect image{content_rect.x - static_cast<float>(entry.content.x) * k,
                     content_rect.y - static_cast<float>(entry.content.y) * k,
                     content_rect.w + (iw - static_cast<float>(entry.content.w)) * k,
                     content_rect.h + (ih - static_cast<float>(entry.content.h)) * k};
    if (!drawable(image)) {
        return pieces;
    }
    const Slice9Px& px = *entry.slice9;
    const AxisSplit sx = split_axis(static_cast<float>(px.left) * k,
                                    static_cast<float>(px.right) * k, image.w);
    const AxisSplit sy = split_axis(static_cast<float>(px.top) * k,
                                    static_cast<float>(px.bottom) * k, image.h);
    const std::array<float, 4> xs{image.x, image.x + sx.a, image.x + sx.a + sx.mid,
                                  image.x + sx.a + sx.mid + sx.b};
    const std::array<float, 4> ys{image.y, image.y + sy.a, image.y + sy.a + sy.mid,
                                  image.y + sy.a + sy.mid + sy.b};
    const std::array<float, 4> us{0.0f, static_cast<float>(px.left) / iw,
                                  (iw - static_cast<float>(px.right)) / iw, 1.0f};
    const std::array<float, 4> vs{0.0f, static_cast<float>(px.top) / ih,
                                  (ih - static_cast<float>(px.bottom)) / ih, 1.0f};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t col = 0; col < 3; ++col) {
            pieces[row * 3 + col] = ThemePiece{
                Rect{xs[col], ys[row], xs[col + 1] - xs[col], ys[row + 1] - ys[row]},
                UVRect{us[col], vs[row], us[col + 1], vs[row + 1]}};
        }
    }
    return pieces;
}

Rect frame_image_rect(const ThemeEntry& entry, const Rect& hole_rect) {
    if (!entry.hole || entry.hole->w <= 0 || entry.hole->h <= 0 || !drawable(hole_rect)) {
        return Rect{};
    }
    const PxRect& hole = *entry.hole;
    const float kx = hole_rect.w / static_cast<float>(hole.w);
    const float ky = hole_rect.h / static_cast<float>(hole.h);
    return Rect{hole_rect.x - static_cast<float>(hole.x) * kx,
                hole_rect.y - static_cast<float>(hole.y) * ky,
                static_cast<float>(entry.width) * kx, static_cast<float>(entry.height) * ky};
}

std::array<Rect, 4> frame_ring_rects(const Rect& image_rect, const Rect& hole_rect) {
    const float right = image_rect.x + image_rect.w;
    const float bottom = image_rect.y + image_rect.h;
    const float hole_right = hole_rect.x + hole_rect.w;
    const float hole_bottom = hole_rect.y + hole_rect.h;
    return std::array<Rect, 4>{
        Rect{image_rect.x, image_rect.y, image_rect.w, std::max(0.0f, hole_rect.y - image_rect.y)},
        Rect{image_rect.x, hole_bottom, image_rect.w, std::max(0.0f, bottom - hole_bottom)},
        Rect{image_rect.x, hole_rect.y, std::max(0.0f, hole_rect.x - image_rect.x), hole_rect.h},
        Rect{hole_right, hole_rect.y, std::max(0.0f, right - hole_right), hole_rect.h},
    };
}

UVRect tile_uv(const ThemeEntry& entry, const Rect& rect, float k, TileAnchor anchor) {
    const UVRect empty{0.0f, 0.0f, 0.0f, 0.0f};
    if (!drawable(rect) || entry.width <= 0 || entry.height <= 0) {
        return empty;
    }
    float tile_w = static_cast<float>(entry.width);
    float tile_h = static_cast<float>(entry.height);
    if (!entry.screen_pixel_tile) {
        if (!finite_positive(k)) {
            return empty;
        }
        tile_w *= k;
        tile_h *= k;
    }
    const float repeats_x = rect.w / tile_w;
    const float repeats_y = rect.h / tile_h;
    if (anchor == TileAnchor::Bottom) {
        return UVRect{0.0f, 1.0f - repeats_y, repeats_x, 1.0f};
    }
    return UVRect{0.0f, 0.0f, repeats_x, repeats_y};
}

std::optional<Rect> fill_cropped_rect(const Rect& bar_rect, float fraction) {
    if (!drawable(bar_rect)) {
        return std::nullopt;
    }
    const float f = std::isnan(fraction) ? 0.0f : std::clamp(fraction, 0.0f, 1.0f);
    if (f <= 0.0f) {
        return std::nullopt;
    }
    return Rect{bar_rect.x, bar_rect.y + bar_rect.h * (1.0f - f), bar_rect.w, bar_rect.h * f};
}

std::optional<ThemePiece> fill_cropped_piece(const ThemeEntry& entry, const Rect& bar_rect,
                                             float fraction) {
    if (!has_content(entry)) {
        return std::nullopt;
    }
    const std::optional<Rect> dst = fill_cropped_rect(bar_rect, fraction);
    if (!dst) {
        return std::nullopt;
    }
    const float f = std::clamp(fraction, 0.0f, 1.0f);
    const float iw = static_cast<float>(entry.width);
    const float ih = static_cast<float>(entry.height);
    const float u0 = static_cast<float>(entry.content.x) / iw;
    const float u1 = static_cast<float>(entry.content.x + entry.content.w) / iw;
    const float vc0 = static_cast<float>(entry.content.y) / ih;
    const float vc1 = static_cast<float>(entry.content.y + entry.content.h) / ih;
    return ThemePiece{*dst, UVRect{u0, vc1 - f * (vc1 - vc0), u1, vc1}};
}

Color theme_fallback_color(ThemeKind kind, Color tint) {
    if (kind == ThemeKind::Fullscreen) {
        return multiply(theme::color::kNavyDeep, tint);
    }
    return with_alpha(tint, tint.a * kFallbackAlphaScale);
}

ThemeLoadOptions theme_texture_options(std::string_view name, const ThemeEntry& entry) {
    ThemeLoadOptions options;
    options.mipmaps =
        name == "logo" || name.starts_with("grade_") || name.starts_with("judgment_");
    if (entry.kind == ThemeKind::Tile) {
        options.wrap = Texture::Wrap::Repeat;
    }
    if (entry.screen_pixel_tile) {
        options.filter = Texture::Filter::Nearest;
    }
    return options;
}

float digits_width(const DigitFont& font, std::string_view text, float k) {
    if (!finite_positive(k)) {
        return 0.0f;
    }
    float width = 0.0f;
    for (const char c : text) {
        const int index = digit_glyph_index(c);
        if (index >= 0 && font.glyphs[static_cast<std::size_t>(index)].present) {
            width += font.glyphs[static_cast<std::size_t>(index)].advance * k;
        }
    }
    return width;
}

float digits_start_x(float x, float width, DigitAlign align) {
    switch (align) {
    case DigitAlign::Centre:
        return x - width * 0.5f;
    case DigitAlign::Right:
        return x - width;
    case DigitAlign::Left:
    default:
        return x;
    }
}

Rect digit_glyph_rect(const DigitGlyph& glyph, float pen_x, float y, float k) {
    return Rect{pen_x - glyph.origin_x * k, y, static_cast<float>(glyph.src.w) * k,
                static_cast<float>(glyph.src.h) * k};
}

Rect digit_fallback_rect(const DigitGlyph& glyph, float pen_x, float y, float k) {
    return Rect{pen_x, y, glyph.advance * k, static_cast<float>(glyph.src.h) * k};
}

UVRect digit_glyph_uv(const DigitGlyph& glyph, int atlas_w, int atlas_h) {
    if (atlas_w <= 0 || atlas_h <= 0) {
        return UVRect{};
    }
    const float w = static_cast<float>(atlas_w);
    const float h = static_cast<float>(atlas_h);
    return UVRect{static_cast<float>(glyph.src.x) / w, static_cast<float>(glyph.src.y) / h,
                  static_cast<float>(glyph.src.x + glyph.src.w) / w,
                  static_cast<float>(glyph.src.y + glyph.src.h) / h};
}

// ---------------------------------------------------------------------------------------------
// BitmapDigits
// ---------------------------------------------------------------------------------------------

void BitmapDigits::reset(std::string name) {
    name_ = std::move(name);
    font_ = DigitFont{};
    font_present_ = false;
    texture_.destroy();
    atlas_w_ = 0;
    atlas_h_ = 0;
    texture_scale_ = kDefaultTextureScale;
    warned_unsupported_ = false;
    warned_missing_ = false;
}

float BitmapDigits::measure(std::string_view text, float s) const {
    if (!font_present_ || !finite_positive(s)) {
        return 0.0f;
    }
    return digits_width(font_, text, s / texture_scale_);
}

float BitmapDigits::height(float s) const {
    if (!font_present_ || !finite_positive(s)) {
        return 0.0f;
    }
    int tallest = 0;
    for (const DigitGlyph& glyph : font_.glyphs) {
        if (glyph.present) {
            tallest = std::max(tallest, glyph.src.h);
        }
    }
    return static_cast<float>(tallest) * (s / texture_scale_);
}

void BitmapDigits::draw(GlQuadRenderer& renderer, std::string_view text, float x, float y,
                        float s, DigitAlign align, Color tint) const {
    if (!font_present_) {
        if (!warned_missing_) {
            warned_missing_ = true;
            std::cerr << "[ThemeTextures] Digit font '" << name_
                      << "' unavailable; digits are not drawn\n";
        }
        return;
    }
    if (!finite_positive(s) || !std::isfinite(x) || !std::isfinite(y)) {
        return;
    }
    const float k = s / texture_scale_;
    float pen = digits_start_x(x, digits_width(font_, text, k), align);
    const Color fallback = theme_fallback_color(ThemeKind::Sprite, tint);
    bool unsupported = false;
    for (const char c : text) {
        const int index = digit_glyph_index(c);
        if (index < 0 || !font_.glyphs[static_cast<std::size_t>(index)].present) {
            unsupported = true;
            continue;
        }
        const DigitGlyph& glyph = font_.glyphs[static_cast<std::size_t>(index)];
        if (texture_.valid()) {
            draw_textured(renderer, digit_glyph_rect(glyph, pen, y, k), texture_,
                          digit_glyph_uv(glyph, atlas_w_, atlas_h_), tint);
        } else if (c != ' ') {
            draw_flat(renderer, digit_fallback_rect(glyph, pen, y, k), fallback);
        }
        pen += glyph.advance * k;
    }
    if (unsupported && !warned_unsupported_) {
        warned_unsupported_ = true;
        std::cerr << "[ThemeTextures] Digit font '" << name_ << "' has no glyph for part of \""
                  << text << "\" (drawn with zero width; logged once)\n";
    }
}

// ---------------------------------------------------------------------------------------------
// ThemeTextures
// ---------------------------------------------------------------------------------------------

ThemeTextures::ThemeTextures() {
    digits_chrome_.reset("digits_chrome");
    digits_white_.reset("digits_white");
}

std::filesystem::path ThemeTextures::default_directory() {
    const std::filesystem::path relative =
        std::filesystem::path("assets") / "theme" / "cabinet" / "manifest.json";
    const std::filesystem::path manifest = resolve_first_existing({
        relative,
        default_executable_dir() / relative,
    });
    return manifest.empty() ? std::filesystem::path{} : manifest.parent_path();
}

void ThemeTextures::shutdown() {
    slots_.clear();
    texture_scale_ = kDefaultTextureScale;
    digits_chrome_.reset("digits_chrome");
    digits_white_.reset("digits_white");
    warned_.clear();
}

bool ThemeTextures::load(const std::filesystem::path& directory) {
    shutdown();
    if (directory.empty()) {
        std::cerr << "[ThemeTextures] Theme directory not found (assets/theme/cabinet); "
                     "drawing flat fallbacks\n";
        return false;
    }

    const std::filesystem::path manifest_path = directory / "manifest.json";
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(manifest_path, ec);
    if (ec || size > kMaxManifestBytes) {
        std::cerr << "[ThemeTextures] Missing, unreadable or oversize manifest: "
                  << manifest_path.string() << "; drawing flat fallbacks\n";
        return false;
    }
    std::ifstream file(manifest_path, std::ios::binary);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    if (!file) {
        std::cerr << "[ThemeTextures] Could not read manifest: " << manifest_path.string()
                  << "; drawing flat fallbacks\n";
        return false;
    }

    std::vector<std::string> warnings;
    ThemeManifest manifest = parse_theme_manifest(buffer.str(), &warnings);
    for (const std::string& warning : warnings) {
        std::cerr << "[ThemeTextures] " << warning << "\n";
    }
    if (manifest.textures.empty() && manifest.fonts.empty()) {
        if (warnings.empty()) {
            std::cerr << "[ThemeTextures] Manifest lists no textures: " << manifest_path.string()
                      << "\n";
        }
        return false;
    }
    texture_scale_ = manifest.texture_scale;

    // Probe every PNG header (no decode) so a missing or mis-sized file is
    // reported once at load, then keep the entry for fallback geometry.
    std::vector<std::pair<std::string_view, Slot*>> uploads;
    uploads.reserve(manifest.textures.size());
    for (auto& [name, entry] : manifest.textures) {
        const std::string path = (directory / entry.file).string();
        const ImageHeader header = probe_image_header(path);
        auto [it, inserted] = slots_.emplace(name, Slot{std::move(entry), Texture{}});
        (void)inserted;
        if (!header.ok) {
            std::cerr << "[ThemeTextures] Missing or unreadable texture: " << it->second.entry.file
                      << " ('" << name << "' draws a flat fallback)\n";
        } else if (header.width != it->second.entry.width ||
                   header.height != it->second.entry.height) {
            std::cerr << "[ThemeTextures] Texture " << it->second.entry.file << " is "
                      << header.width << "x" << header.height << " but the manifest says "
                      << it->second.entry.width << "x" << it->second.entry.height << " ('" << name
                      << "' draws a flat fallback)\n";
        } else {
            uploads.emplace_back(it->first, &it->second);
        }
    }

    std::vector<BitmapDigits*> font_uploads;
    for (auto& [name, font] : manifest.fonts) {
        BitmapDigits* target = name == "digits_chrome" ? &digits_chrome_
                               : name == "digits_white" ? &digits_white_
                                                        : nullptr;
        if (target == nullptr) {
            continue; // not a font this module exposes
        }
        target->reset(name);
        target->font_ = std::move(font);
        target->texture_scale_ = texture_scale_;
        const ImageHeader header = probe_image_header((directory / target->font_.file).string());
        if (!header.ok) {
            std::cerr << "[ThemeTextures] Missing or unreadable digit atlas: "
                      << target->font_.file << " ('" << name << "' draws flat glyphs)\n";
        } else {
            target->atlas_w_ = header.width;
            target->atlas_h_ = header.height;
            for (std::size_t i = 0; i < target->font_.glyphs.size(); ++i) {
                DigitGlyph& glyph = target->font_.glyphs[i];
                if (glyph.present && !rect_inside(glyph.src, header.width, header.height)) {
                    glyph = DigitGlyph{};
                    std::cerr << "[ThemeTextures] Digit font '" << name << "': glyph '"
                              << kDigitChars[i] << "' lies outside the atlas (dropped)\n";
                }
            }
            font_uploads.push_back(target);
        }
        target->font_present_ = std::any_of(target->font_.glyphs.begin(),
                                            target->font_.glyphs.end(),
                                            [](const DigitGlyph& glyph) { return glyph.present; });
    }

    if (glad_glGenTextures == nullptr) {
        std::cerr << "[ThemeTextures] No GL context available; drawing flat fallbacks for "
                  << slots_.size() << " theme textures\n";
        return true;
    }

    for (auto& [name, slot] : uploads) {
        const ThemeLoadOptions options = theme_texture_options(name, slot->entry);
        slot->texture = Texture::from_file((directory / slot->entry.file).string(),
                                           options.mipmaps, options.wrap, options.filter);
        if (!slot->texture.valid()) {
            std::cerr << "[ThemeTextures] Could not load texture: " << slot->entry.file << " ('"
                      << name << "' draws a flat fallback)\n";
        }
    }
    std::size_t fonts_loaded = 0;
    for (BitmapDigits* target : font_uploads) {
        const ThemeLoadOptions options = theme_texture_options(target->name_, ThemeEntry{});
        target->texture_ = Texture::from_file((directory / target->font_.file).string(),
                                              options.mipmaps, options.wrap, options.filter);
        if (target->texture_.valid()) {
            ++fonts_loaded;
        } else {
            std::cerr << "[ThemeTextures] Could not load digit atlas: " << target->font_.file
                      << " ('" << target->name_ << "' draws flat glyphs)\n";
        }
    }

    std::cout << "[ThemeTextures] Loaded " << loaded_texture_count() << "/" << slots_.size()
              << " textures, " << fonts_loaded << "/" << manifest.fonts.size()
              << " digit fonts from " << directory.string() << "\n";
    return true;
}

std::size_t ThemeTextures::loaded_texture_count() const {
    return static_cast<std::size_t>(std::count_if(
        slots_.begin(), slots_.end(), [](const auto& item) { return item.second.texture.valid(); }));
}

const ThemeEntry* ThemeTextures::entry(std::string_view name) const {
    const auto it = slots_.find(name);
    return it == slots_.end() ? nullptr : &it->second.entry;
}

Vec2 ThemeTextures::content_size(std::string_view name, float s) const {
    const ThemeEntry* found = entry(name);
    const float k = scale_k(s);
    if (found == nullptr || k <= 0.0f) {
        return Vec2{};
    }
    return Vec2{static_cast<float>(found->content.w) * k, static_cast<float>(found->content.h) * k};
}

const ThemeTextures::Slot* ThemeTextures::find(std::string_view name) const {
    const auto it = slots_.find(name);
    if (it != slots_.end()) {
        return &it->second;
    }
    if (warned_.find(name) == warned_.end()) {
        warned_.emplace(name);
        std::cerr << "[ThemeTextures] Unknown theme texture '" << name
                  << "' (flat fallback or nothing drawn; logged once)\n";
    }
    return nullptr;
}

float ThemeTextures::scale_k(float s) const {
    return finite_positive(s) ? s / texture_scale_ : 0.0f;
}

void ThemeTextures::draw_sprite(GlQuadRenderer& renderer, std::string_view name, Vec2 content_pos,
                                float s, Color tint) const {
    const Slot* slot = find(name);
    if (slot == nullptr) {
        return; // no geometry without the manifest entry
    }
    const Rect content = sprite_content_rect(slot->entry, content_pos, scale_k(s));
    if (!drawable(content)) {
        return;
    }
    if (slot->texture.valid()) {
        draw_textured(renderer, content_to_image_rect(slot->entry, content), slot->texture,
                      UVRect{}, tint);
    } else {
        draw_flat(renderer, content, theme_fallback_color(slot->entry.kind, tint));
    }
}

void ThemeTextures::draw_stretch(GlQuadRenderer& renderer, std::string_view name,
                                 const Rect& content_rect, Color tint) const {
    const Slot* slot = find(name);
    if (slot == nullptr) {
        draw_flat(renderer, content_rect, unknown_fallback_color(tint));
        return;
    }
    draw_stretched(renderer, slot->entry, slot->texture, content_rect, tint);
}

void ThemeTextures::draw_stretch_x(GlQuadRenderer& renderer, std::string_view name, float x,
                                   float y, float width, float s, Color tint) const {
    const Slot* slot = find(name);
    const float k = scale_k(s);
    if (slot == nullptr || k <= 0.0f) {
        return; // no height without the manifest entry
    }
    const Rect content{x, y, width, static_cast<float>(slot->entry.content.h) * k};
    draw_stretched(renderer, slot->entry, slot->texture, content, tint);
}

void ThemeTextures::draw_slice3(GlQuadRenderer& renderer, std::string_view name,
                                const Rect& content_rect, Color tint) const {
    const Slot* slot = find(name);
    if (slot == nullptr) {
        draw_flat(renderer, content_rect, unknown_fallback_color(tint));
        return;
    }
    if (!slot->texture.valid()) {
        draw_flat(renderer, content_rect, theme_fallback_color(slot->entry.kind, tint));
        return;
    }
    for (const ThemePiece& piece : slice3_pieces(slot->entry, content_rect)) {
        draw_textured(renderer, piece.dst, slot->texture, piece.uv, tint);
    }
}

void ThemeTextures::draw_slice9(GlQuadRenderer& renderer, std::string_view name,
                                const Rect& content_rect, float s, Color tint) const {
    const Slot* slot = find(name);
    if (slot == nullptr) {
        draw_flat(renderer, content_rect, unknown_fallback_color(tint));
        return;
    }
    if (!slot->texture.valid()) {
        draw_flat(renderer, content_rect, theme_fallback_color(slot->entry.kind, tint));
        return;
    }
    for (const ThemePiece& piece : slice9_pieces(slot->entry, content_rect, scale_k(s))) {
        draw_textured(renderer, piece.dst, slot->texture, piece.uv, tint);
    }
}

void ThemeTextures::draw_frame(GlQuadRenderer& renderer, std::string_view name,
                               const Rect& hole_rect, Color tint) const {
    const Slot* slot = find(name);
    if (slot == nullptr) {
        draw_flat(renderer, hole_rect, unknown_fallback_color(tint));
        return;
    }
    const Rect image = frame_image_rect(slot->entry, hole_rect);
    if (!drawable(image)) {
        return;
    }
    if (slot->texture.valid()) {
        draw_textured(renderer, image, slot->texture, UVRect{}, tint);
        return;
    }
    const Color color = theme_fallback_color(slot->entry.kind, tint);
    for (const Rect& strip : frame_ring_rects(image, hole_rect)) {
        draw_flat(renderer, strip, color);
    }
}

void ThemeTextures::draw_tiled(GlQuadRenderer& renderer, std::string_view name, const Rect& rect,
                               float s, Color tint, TileAnchor anchor) const {
    const Slot* slot = find(name);
    if (slot == nullptr) {
        draw_flat(renderer, rect, unknown_fallback_color(tint));
        return;
    }
    if (!slot->texture.valid()) {
        draw_flat(renderer, rect, theme_fallback_color(slot->entry.kind, tint));
        return;
    }
    const UVRect uv = tile_uv(slot->entry, rect, scale_k(s), anchor);
    if (uv.u1 <= uv.u0 || uv.v1 <= uv.v0) {
        return;
    }
    draw_textured(renderer, rect, slot->texture, uv, tint);
}

void ThemeTextures::draw_fill_cropped(GlQuadRenderer& renderer, std::string_view name,
                                      const Rect& bar_rect, float fraction, Color tint) const {
    const Slot* slot = find(name);
    if (slot == nullptr) {
        if (const std::optional<Rect> fill = fill_cropped_rect(bar_rect, fraction)) {
            draw_flat(renderer, *fill, unknown_fallback_color(tint));
        }
        return;
    }
    const std::optional<ThemePiece> piece = fill_cropped_piece(slot->entry, bar_rect, fraction);
    if (!piece) {
        return;
    }
    if (slot->texture.valid()) {
        draw_textured(renderer, piece->dst, slot->texture, piece->uv, tint);
    } else {
        draw_flat(renderer, piece->dst, theme_fallback_color(slot->entry.kind, tint));
    }
}

} // namespace blaze4k
