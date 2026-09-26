#pragma once

#include <cstdint>
#include "render/geometry.hpp"

namespace td {

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
    static Texture from_rgba(int width, int height, const uint8_t* rgba);

    // Builds a 1x1 texture filled with `color` (useful for tinted solid quads).
    static Texture solid(Color color);

    void destroy();

    [[nodiscard]] bool valid() const { return id_ != 0; }
    [[nodiscard]] unsigned int id() const { return id_; }
    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }

private:
    unsigned int id_ = 0;
    int width_ = 0;
    int height_ = 0;
};

} // namespace td
