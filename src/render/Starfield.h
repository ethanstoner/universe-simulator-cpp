#pragma once

#include <glm/mat4x4.hpp>

#include "render/Mesh.h"
#include "render/RenderSettings.h"
#include "render/Shader.h"

namespace render {

// A procedural background starfield.
//
// Purely decorative: it is not a star catalogue and the stars are not bodies in
// the simulation. Generated once from a fixed seed so a given build always
// shows the same sky, which keeps screenshot comparisons meaningful.
class Starfield {
public:
    bool initialize(int count = 2600);
    bool reloadShader() { return shader_.reload(); }

    // Drawn before everything else with depth writes off, so the whole scene
    // composites over it.
    void draw(const glm::mat4& viewProjection, const RenderSettings& settings);

    int count() const { return count_; }

private:
    void build(int count);

    Shader shader_;
    Mesh mesh_;
    int count_ = 0;
};

}  // namespace render
