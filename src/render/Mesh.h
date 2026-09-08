#pragma once

#include <cstdint>
#include <vector>

#include <glm/vec3.hpp>

namespace render {

struct Vertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
};

enum class DrawMode { Triangles, Lines, LineStrip, Points };

// Owns one VAO/VBO(/EBO). Index data is optional; meshes without it draw with
// glDrawArrays. Buffers can be re-uploaded in place, which the trail and grid
// renderers rely on.
class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void upload(const std::vector<Vertex>& vertices,
                const std::vector<std::uint32_t>& indices = {},
                bool dynamic = false);

    // Replaces vertex data without reallocating when the new data fits.
    void updateVertices(const std::vector<Vertex>& vertices);

    void draw(DrawMode mode = DrawMode::Triangles) const;
    // Draws only the first `count` vertices/indices; used by the trail renderer
    // which keeps one big buffer and fills a varying prefix of it.
    void drawRange(DrawMode mode, std::size_t count) const;

    bool valid() const { return vao_ != 0; }
    std::size_t vertexCount() const { return vertexCount_; }
    std::size_t indexCount() const { return indexCount_; }

    void release();

private:
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int ebo_ = 0;
    std::size_t vertexCount_ = 0;
    std::size_t indexCount_ = 0;
    std::size_t vertexCapacity_ = 0;
    bool dynamic_ = false;
};

}  // namespace render
