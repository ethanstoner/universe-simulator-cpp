#pragma once

#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "render/Mesh.h"
#include "render/Shader.h"

namespace render {

// Batches arbitrary line segments for one frame. Used for velocity and
// acceleration arrows, the Schwarzschild radius marker, the spawn preview and
// the bounds box. Everything is submitted in camera-relative world units.
class LineRenderer {
public:
    bool initialize();
    bool reloadShader() { return shader_.reload(); }

    void begin();
    void addSegment(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color);
    void addRing(const glm::vec3& centre, double radius, const glm::vec3& normal,
                 const glm::vec4& color, int segments = 64);
    void addBox(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color);
    // An arrow drawn as a shaft plus two head strokes in the camera's plane.
    void addArrow(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color,
                  const glm::vec3& cameraRight, const glm::vec3& cameraUp);
    void flush(const glm::mat4& viewProjection);

private:
    // Colour is packed per-vertex, so a whole frame of differently coloured
    // lines is one draw call.
    struct LineVertex {
        glm::vec3 position;
        glm::vec3 color;
        float alpha;
    };

    Shader shader_;
    Mesh mesh_;
    std::vector<LineVertex> vertices_;
    std::vector<Vertex> upload_;
    std::vector<glm::vec4> colors_;
};

}  // namespace render
