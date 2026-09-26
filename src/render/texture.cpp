#include "render/texture.hpp"

#include <iostream>
#include <utility>

#include <glad/glad.h>

namespace td {

namespace {

bool gl_available() {
    return glad_glGenTextures != nullptr;
}

} // namespace

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

Texture Texture::from_rgba(int width, int height, const uint8_t* rgba) {
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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

} // namespace td
