#include "render/texture.hpp"

#include <filesystem>
#include <iostream>
#include <utility>

#include <glad/glad.h>
#include <stb_image.h>

namespace td {

namespace {

// Untrusted-input caps (AGENTS.md parser-hardening discipline). The
// dimension cap is enforced on the decoded *header* (stbi_info) before any
// pixel allocation; kMaxImageBytes only bounds the read size.
constexpr std::uintmax_t kMaxImageBytes = 16u * 1024u * 1024u; // 16 MiB
constexpr int kMaxImageDimension = 4096;

bool gl_available() {
    return glad_glGenTextures != nullptr;
}

} // namespace

ImageHeader probe_image_header(const std::string& path) {
    ImageHeader header;
    int channels = 0;
    if (!stbi_info(path.c_str(), &header.width, &header.height, &channels)) {
        header.width = 0;
        header.height = 0;
        return header;
    }
    header.ok = header.width > 0 && header.height > 0 && header.width <= kMaxImageDimension &&
                header.height <= kMaxImageDimension;
    return header;
}

Texture::~Texture() {
    destroy();
}

Texture::Texture(Texture&& other) noexcept
    : id_(other.id_), width_(other.width_), height_(other.height_) {
    other.id_ = 0;
    other.width_ = 0;
    other.height_ = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        destroy();
        id_ = other.id_;
        width_ = other.width_;
        height_ = other.height_;
        other.id_ = 0;
        other.width_ = 0;
        other.height_ = 0;
    }
    return *this;
}

void Texture::destroy() {
    if (id_ != 0 && gl_available()) {
        glDeleteTextures(1, &id_);
    }
    id_ = 0;
    width_ = 0;
    height_ = 0;
}

Texture Texture::from_rgba(int width, int height, const uint8_t* rgba, bool mipmaps) {
    Texture texture;

    if (width <= 0 || height <= 0 || rgba == nullptr) {
        std::cerr << "[Texture] Invalid RGBA upload request (" << width << "x" << height << ")\n";
        return texture;
    }
    if (!gl_available()) {
        std::cerr << "[Texture] No OpenGL context available; skipping texture upload\n";
        return texture;
    }

    GLuint id = 0;
    glGenTextures(1, &id);
    if (id == 0) {
        std::cerr << "[Texture] glGenTextures failed\n";
        return texture;
    }

    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    if (mipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    texture.id_ = id;
    texture.width_ = width;
    texture.height_ = height;
    return texture;
}

Texture Texture::solid(Color color) {
    const auto to_byte = [](float value) -> uint8_t {
        if (value <= 0.0f) {
            return 0;
        }
        if (value >= 1.0f) {
            return 255;
        }
        return static_cast<uint8_t>(value * 255.0f + 0.5f);
    };

    const uint8_t rgba[4] = {
        to_byte(color.r),
        to_byte(color.g),
        to_byte(color.b),
        to_byte(color.a),
    };
    return from_rgba(1, 1, rgba);
}

Texture Texture::from_file(const std::string& path, bool mipmaps) {
    Texture texture;

    if (path.empty()) {
        std::cerr << "[Texture] from_file called with an empty path\n";
        return texture;
    }

    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec) {
        std::cerr << "[Texture] Cannot stat image file: " << path << "\n";
        return texture;
    }
    if (size == 0 || size > kMaxImageBytes) {
        std::cerr << "[Texture] Rejecting image '" << path << "' (size " << size << " bytes)\n";
        return texture;
    }

    // Probe the header before decoding: stb_image's default STBI_MAX_DIMENSIONS
    // (1 << 24) would otherwise let a kilobyte-sized PNG declaring 16000x16000
    // allocate ~1 GiB in stbi_load before a post-decode check runs.
    const ImageHeader header = probe_image_header(path);
    if (!header.ok) {
        std::cerr << "[Texture] Rejecting image '" << path << "' (header " << header.width << "x"
                  << header.height << ", cap " << kMaxImageDimension << "px)\n";
        return texture;
    }

    if (!gl_available()) {
        std::cerr << "[Texture] No OpenGL context available; skipping image decode: " << path
                  << "\n";
        return texture;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (pixels == nullptr) {
        std::cerr << "[Texture] Failed to decode image '" << path
                  << "': " << stbi_failure_reason() << "\n";
        return texture;
    }

    texture = from_rgba(width, height, pixels, mipmaps);
    stbi_image_free(pixels);
    return texture;
}

} // namespace td
