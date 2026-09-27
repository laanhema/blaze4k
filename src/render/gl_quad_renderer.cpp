#include "render/gl_quad_renderer.hpp"

#include <cmath>
#include <iostream>

#include <glad/glad.h>

namespace td {

namespace {

const char* kVertexShader = R"(#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_color;
uniform mat4 u_projection;
out vec2 v_uv;
out vec4 v_color;
void main() {
    v_uv = a_uv;
    v_color = a_color;
    gl_Position = u_projection * vec4(a_pos, 0.0, 1.0);
}
)";

const char* kFragmentShader = R"(#version 330 core
in vec2 v_uv;
in vec4 v_color;
out vec4 FragColor;
uniform sampler2D u_tex;
void main() {
    FragColor = texture(u_tex, v_uv) * v_color;
}
)";

bool gl_available() {
    return glad_glCreateShader != nullptr;
}

unsigned int compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    if (shader == 0) {
        return 0;
    }
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        char log[1024] = {0};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "[GlQuadRenderer] Shader compilation failed: " << log << "\n";
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

} // namespace

GlQuadRenderer::~GlQuadRenderer() {
    shutdown();
}

bool GlQuadRenderer::init() {
    if (initialized_) {
        return true;
    }
    if (!gl_available()) {
        std::cerr << "[GlQuadRenderer] No OpenGL context available; renderer disabled\n";
        return false;
    }

    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, kVertexShader);
    if (vertex_shader == 0) {
        return false;
    }
    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (fragment_shader == 0) {
        glDeleteShader(vertex_shader);
        return false;
    }

    program_ = glCreateProgram();
    glAttachShader(program_, vertex_shader);
    glAttachShader(program_, fragment_shader);
    glLinkProgram(program_);

    GLint linked = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char log[1024] = {0};
        glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
        std::cerr << "[GlQuadRenderer] Program link failed: " << log << "\n";
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        glDeleteProgram(program_);
        program_ = 0;
        return false;
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    projection_location_ = glGetUniformLocation(program_, "u_projection");
    texture_location_ = glGetUniformLocation(program_, "u_tex");

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    const GLsizei stride = static_cast<GLsizei>(sizeof(Vertex));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(sizeof(float) * 2));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(sizeof(float) * 4));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    white_ = Texture::solid(Color{1.0f, 1.0f, 1.0f, 1.0f});
    if (!white_.valid()) {
        std::cerr << "[GlQuadRenderer] Failed to create white texture\n";
        shutdown();
        return false;
    }

    initialized_ = true;
    return true;
}

void GlQuadRenderer::shutdown() {
    if (vbo_ != 0 && gl_available()) {
        glDeleteBuffers(1, &vbo_);
    }
    if (vao_ != 0 && gl_available()) {
        glDeleteVertexArrays(1, &vao_);
    }
    if (program_ != 0 && gl_available()) {
        glDeleteProgram(program_);
    }
    vbo_ = 0;
    vao_ = 0;
    program_ = 0;
    white_.destroy();
    vertices_.clear();
    bound_texture_ = 0;
    initialized_ = false;
}

void GlQuadRenderer::build_projection(int width, int height) {
    // Maps x:[0,w] -> [-1,1] and y:[0,h] -> [1,-1] (top-left origin, y down).
    const float w = (width > 0) ? static_cast<float>(width) : 1.0f;
    const float h = (height > 0) ? static_cast<float>(height) : 1.0f;

    projection_[0] = 2.0f / w;  projection_[1] = 0.0f;         projection_[2] = 0.0f;   projection_[3] = 0.0f;
    projection_[4] = 0.0f;      projection_[5] = -2.0f / h;    projection_[6] = 0.0f;   projection_[7] = 0.0f;
    projection_[8] = 0.0f;      projection_[9] = 0.0f;         projection_[10] = -1.0f; projection_[11] = 0.0f;
    projection_[12] = -1.0f;    projection_[13] = 1.0f;        projection_[14] = 0.0f;  projection_[15] = 1.0f;
}

void GlQuadRenderer::begin(int framebuffer_width, int framebuffer_height) {
    if (!initialized_) {
        return;
    }

    glViewport(0, 0, framebuffer_width, framebuffer_height);
    build_projection(framebuffer_width, framebuffer_height);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    blend_mode_ = BlendMode::Alpha;

    glUseProgram(program_);
    glUniformMatrix4fv(projection_location_, 1, GL_FALSE, projection_);
    glUniform1i(texture_location_, 0);

    vertices_.clear();
    bound_texture_ = 0;
}

void GlQuadRenderer::set_blend_mode(BlendMode mode) {
    if (!initialized_ || blend_mode_ == mode) {
        return;
    }
    flush();
    blend_mode_ = mode;
    if (mode == BlendMode::Add) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    } else {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
}

void GlQuadRenderer::bind_texture(unsigned int id) {
    if (bound_texture_ == id) {
        return;
    }
    flush();
    bound_texture_ = id;
}

void GlQuadRenderer::append_quad(const Rect& rect, const UVRect& uv, Color color, float radians) {
    const float x0 = rect.x;
    const float y0 = rect.y;
    const float x1 = rect.x + rect.w;
    const float y1 = rect.y + rect.h;

    Vertex top_left{x0, y0, uv.u0, uv.v0, color.r, color.g, color.b, color.a};
    Vertex top_right{x1, y0, uv.u1, uv.v0, color.r, color.g, color.b, color.a};
    Vertex bottom_right{x1, y1, uv.u1, uv.v1, color.r, color.g, color.b, color.a};
    Vertex bottom_left{x0, y1, uv.u0, uv.v1, color.r, color.g, color.b, color.a};

    if (radians != 0.0f) {
        const float cx = rect.x + rect.w * 0.5f;
        const float cy = rect.y + rect.h * 0.5f;
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        for (Vertex* v : {&top_left, &top_right, &bottom_right, &bottom_left}) {
            const float dx = v->x - cx;
            const float dy = v->y - cy;
            v->x = cx + dx * c - dy * s;
            v->y = cy + dx * s + dy * c;
        }
    }

    vertices_.push_back(top_left);
    vertices_.push_back(top_right);
    vertices_.push_back(bottom_right);
    vertices_.push_back(top_left);
    vertices_.push_back(bottom_right);
    vertices_.push_back(bottom_left);
}

void GlQuadRenderer::draw_quad(const Rect& rect, Color color) {
    if (!initialized_) {
        return;
    }
    bind_texture(white_.id());
    append_quad(rect, UVRect{}, color);
}

void GlQuadRenderer::draw_textured_quad(const Rect& rect, const Texture& texture, const UVRect& uv, Color color) {
    draw_textured_quad(rect, texture, uv, color, 0.0f);
}

void GlQuadRenderer::draw_textured_quad(const Rect& rect, const Texture& texture, const UVRect& uv,
                                        Color color, float radians) {
    if (!initialized_) {
        return;
    }
    const unsigned int id = texture.valid() ? texture.id() : white_.id();
    bind_texture(id);
    append_quad(rect, uv, color, radians);
}

void GlQuadRenderer::flush() {
    if (!initialized_ || vertices_.empty()) {
        return;
    }

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)),
                 vertices_.data(),
                 GL_DYNAMIC_DRAW);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, bound_texture_ != 0 ? bound_texture_ : white_.id());

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices_.size()));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    vertices_.clear();
}

void GlQuadRenderer::end() {
    if (!initialized_) {
        return;
    }
    flush();
    set_blend_mode(BlendMode::Alpha);
    bound_texture_ = 0;
}

} // namespace td
