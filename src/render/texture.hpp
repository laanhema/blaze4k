#pragma once

#include <cstdint>
#include <span>
#include <string>
#include "render/geometry.hpp"

namespace blaze4k {

// Cheap, non-decoding result of probing an image header (stbi_info).
struct ImageHeader {
    // True only when the header parsed and the dimensions are positive and
    // within the untrusted-input cap (<= 4096px).
    bool ok = false;
    int width = 0;
    int height = 0;
};

// Probes an image header without decoding pixels. Returns ok=false when the
// file cannot be read/parsed or declares non-positive/oversized dimensions, so
// callers can reject untrusted images before any pixel allocation. Never throws.
[[nodiscard]] ImageHeader probe_image_header(const std::string& path);

// Multiplies each RGBA8 pixel's RGB by its alpha in place (round to nearest).
// Processes `rgba.size() / 4` whole pixels and ignores any trailing partial
// pixel. Pure (no GL).
void premultiply_alpha(std::span<std::uint8_t> rgba);

// Move-only RAII wrapper around an OpenGL 2D texture.
//
// Construction is only possible through the static factories below. When no GL
// context is bound (headless mode) the factories log a `[Texture]` warning and
// return an invalid texture instead of calling into unloaded GL entry points.
class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    // Uploads tightly-packed RGBA8 pixels. Returns an invalid texture on bad input.
    // `rgba` is straight (non-premultiplied) alpha; the texture stores
    // premultiplied alpha so linear/mipmap filtering cannot bleed the RGB of
    // transparent texels into edges (#59).
    // `mipmaps` builds a mip chain with trilinear minification, for art drawn
    // well below its native size (e.g. 128px noteskin frames drawn at 56px).
    static Texture from_rgba(int width, int height, const uint8_t* rgba, bool mipmaps = false);

    // Builds a 1x1 texture filled with `color` (useful for tinted solid quads).
    static Texture solid(Color color);

    // Decodes a PNG/JPG/BMP file (via stb_image) and uploads it as RGBA8.
    // Untrusted input: returns an invalid texture (never throws) for an empty
    // path, a missing/oversize (> kMaxImageBytes) file, a header declaring
    // dimensions over the cap (rejected via stbi_info before any decode), a
    // decode failure, or when no GL context is available (headless).
    // The decoded pixels are straight alpha; like from_rgba, the texture stores
    // premultiplied alpha so filtering cannot bleed transparent RGB into edges (#59).
    static Texture from_file(const std::string& path, bool mipmaps = false);

    void destroy();

    [[nodiscard]] bool valid() const { return id_ != 0; }
    [[nodiscard]] unsigned int id() const { return id_; }
    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }

private:
    // Uploads already-premultiplied RGBA8 bytes as given (arguments validated
    // and GL availability checked by the public factories).
    static Texture upload_premultiplied(int width, int height, const uint8_t* rgba, bool mipmaps);

    unsigned int id_ = 0;
    int width_ = 0;
    int height_ = 0;
};

} // namespace blaze4k
