#pragma once

#include <string>
#include "render/geometry.hpp"
#include "render/texture.hpp"

namespace td {

class GlQuadRenderer;

// Draws the selected song's background behind the note field during gameplay,
// dimmed so the arrows stay readable. The image path is resolved by the caller
// (song art from the scanner, or the committed fallback asset wired through
// `SongLibrary::set_fallback_background`). When no image is loaded (empty path,
// missing/undecodable file, or headless), a dark solid + scrim keeps the field
// readable. 2D textured quads only (PRD locked scope).
class BackgroundRenderer {
public:
    BackgroundRenderer() = default;
    ~BackgroundRenderer();

    BackgroundRenderer(const BackgroundRenderer&) = delete;
    BackgroundRenderer& operator=(const BackgroundRenderer&) = delete;

    // Replaces the current image. A failed load leaves the renderer image-less;
    // it never throws and is safe without a GL context (logs and falls back).
    void load(const std::string& path);
    void render(GlQuadRenderer& renderer, int screen_w, int screen_h) const;
    void shutdown();

    [[nodiscard]] bool has_image() const { return image_.valid(); }
    [[nodiscard]] const Texture& image_texture() const { return image_; }

    // Cover-fit UVs for drawing a `tex_w x tex_h` image into a
    // `screen_w x screen_h` full-screen rect (fill + crop, never stretch).
    // Non-positive dimensions return UVRect{} (full texture).
    [[nodiscard]] static UVRect cover_uv(int screen_w, int screen_h, int tex_w, int tex_h);

private:
    Texture image_;
};

} // namespace td
