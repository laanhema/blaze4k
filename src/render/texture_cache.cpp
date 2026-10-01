#include "render/texture_cache.hpp"

namespace blaze4k {

const Texture* TextureCache::get(const std::string& path) {
    if (path.empty()) {
        return nullptr;
    }

    auto it = textures_.find(path);
    if (it != textures_.end()) {
        return &it->second;
    }

    auto inserted = textures_.emplace(path, Texture::from_file(path));
    return &inserted.first->second;
}

} // namespace blaze4k
