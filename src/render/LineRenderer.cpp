#include "render/LineRenderer.h"

#include <glad/gl.h>

#include <cmath>

namespace render {
namespace {
constexpr float kTwoPi = 6.28318530717958647692f;
}

bool LineRenderer::initialize() {
    return shader_.loadFromFiles("shaders/flat.vert", "shaders/flat.frag");
}

void LineRenderer::begin() {
    vertices_.clear();
    colors_.clear();
}

void LineRenderer::addSegment(const glm::vec3& a, const glm::vec3& b,
                              const glm::vec4& color) {
    vertices_.push_back({a, glm::vec3(color), color.a});
    vertices_.push_back({b, glm::vec3(color), color.a});
}

void LineRenderer::addRing(const glm::vec3& centre, double radius, const glm::vec3& normal,
                           const glm::vec4& color, int segments) {
    if (segments < 3 || radius <= 0.0) return;

    // Build an orthonormal basis in the ring's plane. The fallback axis avoids
    // a degenerate cross product when the normal happens to be the X axis.
    const glm::vec3 n = glm::normalize(normal);
    glm::vec3 reference = std::abs(n.x) < 0.9f ? glm::vec3(1.0f, 0.0f, 0.0f)
                                               : glm::vec3(0.0f, 0.0f, 1.0f);
    const glm::vec3 u = glm::normalize(glm::cross(n, reference));
    const glm::vec3 v = glm::cross(n, u);

    glm::vec3 previous =
        centre + u * static_cast<float>(radius);
    for (int i = 1; i <= segments; ++i) {
        const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
        const glm::vec3 point = centre + (u * std::cos(angle) + v * std::sin(angle)) *
                                             static_cast<float>(radius);
        addSegment(previous, point, color);
        previous = point;
    }
}

void LineRenderer::addBox(const glm::vec3& min, const glm::vec3& max,
                          const glm::vec4& color) {
    const glm::vec3 corners[8] = {
        {min.x, min.y, min.z}, {max.x, min.y, min.z}, {max.x, max.y, min.z},
        {min.x, max.y, min.z}, {min.x, min.y, max.z}, {max.x, min.y, max.z},
        {max.x, max.y, max.z}, {min.x, max.y, max.z}};
    const int edges[24] = {0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6,
                           6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7};
    for (int i = 0; i < 24; i += 2) {
        addSegment(corners[edges[i]], corners[edges[i + 1]], color);
    }
}

void LineRenderer::addArrow(const glm::vec3& from, const glm::vec3& to,
                            const glm::vec4& color, const glm::vec3& cameraRight,
                            const glm::vec3& cameraUp) {
    addSegment(from, to, color);

    const glm::vec3 shaft = to - from;
    const float length = glm::length(shaft);
    if (length <= 1e-6f) return;
    const glm::vec3 direction = shaft / length;

    // Head strokes are laid out in the plane facing the camera so the arrowhead
    // is visible from any angle rather than disappearing when viewed edge on.
    glm::vec3 side = glm::cross(direction, cameraRight);
    if (glm::dot(side, side) < 1e-8f) side = glm::cross(direction, cameraUp);
    if (glm::dot(side, side) < 1e-8f) return;
    side = glm::normalize(side);

    const float headLength = length * 0.18f;
    const glm::vec3 base = to - direction * headLength;
    addSegment(to, base + side * (headLength * 0.5f), color);
    addSegment(to, base - side * (headLength * 0.5f), color);
}

void LineRenderer::flush(const glm::mat4& viewProjection) {
    if (!shader_.valid() || vertices_.empty()) return;

    // The flat shader takes one uniform colour, so runs of identical colour are
    // drawn together. In practice the frame has a handful of distinct colours,
    // so this is a handful of draw calls rather than one per segment.
    shader_.bind();
    shader_.setMat4("uViewProjection", viewProjection);
    shader_.setMat4("uModel", glm::mat4(1.0f));

    std::size_t start = 0;
    while (start < vertices_.size()) {
        const glm::vec3 color = vertices_[start].color;
        const float alpha = vertices_[start].alpha;
        std::size_t end = start;
        while (end < vertices_.size() && vertices_[end].color == color &&
               vertices_[end].alpha == alpha) {
            ++end;
        }

        upload_.clear();
        upload_.reserve(end - start);
        for (std::size_t i = start; i < end; ++i) {
            upload_.push_back({vertices_[i].position, glm::vec3(1.0f)});
        }
        mesh_.updateVertices(upload_);
        shader_.setVec4("uColor", glm::vec4(color, alpha));
        mesh_.drawRange(DrawMode::Lines, upload_.size());

        start = end;
    }
    vertices_.clear();
}

}  // namespace render
