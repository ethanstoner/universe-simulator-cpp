#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "render/Mesh.h"
#include "render/Shader.h"

namespace sim {
class GravitySystem;
}

namespace render {

// Draws each body's position history as a fading line strip.
//
// History is bounded by GravitySystem::trailLength, and this only ever uploads
// what it is about to draw, so there is no unbounded GPU allocation. One
// dynamic buffer is reused for every body rather than one buffer per body.
class TrailRenderer {
public:
    bool initialize();
    bool reloadShader() { return shader_.reload(); }

    void draw(const sim::GravitySystem& system, const glm::dvec3& cameraPosition,
              double metresPerUnit, const glm::mat4& viewProjection, float opacity,
              int maxSamples);

private:
    Shader shader_;
    Mesh mesh_;
    std::vector<Vertex> scratch_;
};

}  // namespace render
