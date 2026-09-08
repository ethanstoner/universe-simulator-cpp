#include "render/Mesh.h"

#include <glad/gl.h>

#include <utility>

namespace render {
namespace {

unsigned int toGlMode(DrawMode mode) {
    switch (mode) {
        case DrawMode::Triangles: return GL_TRIANGLES;
        case DrawMode::Lines: return GL_LINES;
        case DrawMode::LineStrip: return GL_LINE_STRIP;
        case DrawMode::Points: return GL_POINTS;
    }
    return GL_TRIANGLES;
}

}  // namespace

Mesh::~Mesh() { release(); }

Mesh::Mesh(Mesh&& other) noexcept { *this = std::move(other); }

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        release();
        vao_ = std::exchange(other.vao_, 0u);
        vbo_ = std::exchange(other.vbo_, 0u);
        ebo_ = std::exchange(other.ebo_, 0u);
        vertexCount_ = std::exchange(other.vertexCount_, 0u);
        indexCount_ = std::exchange(other.indexCount_, 0u);
        vertexCapacity_ = std::exchange(other.vertexCapacity_, 0u);
        dynamic_ = other.dynamic_;
    }
    return *this;
}

void Mesh::release() {
    if (ebo_) glDeleteBuffers(1, &ebo_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    vao_ = vbo_ = ebo_ = 0;
    vertexCount_ = indexCount_ = vertexCapacity_ = 0;
}

void Mesh::upload(const std::vector<Vertex>& vertices,
                  const std::vector<std::uint32_t>& indices, bool dynamic) {
    if (!vao_) glGenVertexArrays(1, &vao_);
    if (!vbo_) glGenBuffers(1, &vbo_);

    dynamic_ = dynamic;
    const unsigned int usage = dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<long long>(vertices.size() * sizeof(Vertex)),
                 vertices.empty() ? nullptr : vertices.data(), usage);
    vertexCount_ = vertices.size();
    vertexCapacity_ = vertices.size();

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, normal)));

    if (!indices.empty()) {
        if (!ebo_) glGenBuffers(1, &ebo_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<long long>(indices.size() * sizeof(std::uint32_t)),
                     indices.data(), usage);
        indexCount_ = indices.size();
    } else {
        indexCount_ = 0;
    }

    glBindVertexArray(0);
}

void Mesh::updateVertices(const std::vector<Vertex>& vertices) {
    if (!vbo_) {
        upload(vertices, {}, true);
        return;
    }
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    if (vertices.size() > vertexCapacity_) {
        // Grow by reallocating; orphaning the old store lets the driver avoid
        // stalling on in-flight draws.
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<long long>(vertices.size() * sizeof(Vertex)),
                     vertices.data(), GL_DYNAMIC_DRAW);
        vertexCapacity_ = vertices.size();
    } else if (!vertices.empty()) {
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        static_cast<long long>(vertices.size() * sizeof(Vertex)),
                        vertices.data());
    }
    vertexCount_ = vertices.size();
}

void Mesh::draw(DrawMode mode) const {
    drawRange(mode, indexCount_ > 0 ? indexCount_ : vertexCount_);
}

void Mesh::drawRange(DrawMode mode, std::size_t count) const {
    if (!vao_ || count == 0) return;
    glBindVertexArray(vao_);
    if (indexCount_ > 0) {
        if (count > indexCount_) count = indexCount_;
        glDrawElements(toGlMode(mode), static_cast<int>(count), GL_UNSIGNED_INT, nullptr);
    } else {
        if (count > vertexCount_) count = vertexCount_;
        glDrawArrays(toGlMode(mode), 0, static_cast<int>(count));
    }
    glBindVertexArray(0);
}

}  // namespace render
