#pragma once

#include <string>
#include <unordered_map>

#include "render/texture.hpp"

namespace blaze4k {

// Path-keyed texture cache: decodes/uploads each unique banner path at most once
// and hands back a stable pointer to the stored (move-only) Texture. A null
// return means "no path": callers draw a placeholder instead. An invalid
// (headless/decode-failed) texture is still stored so the path is not retried
// every frame; callers must check `Texture::valid()`.
class TextureCache {
public:
    // Returns the cached texture for `path`, decoding it on first use. Returns
    // nullptr for an empty path.
    [[nodiscard]] const Texture* get(const std::string& path);

    void clear() { textures_.clear(); }
    [[nodiscard]] bool empty() const { return textures_.empty(); }

private:
    std::unordered_map<std::string, Texture> textures_;
};

} // namespace blaze4k
