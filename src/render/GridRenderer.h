#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "render/Mesh.h"
#include "render/RenderSettings.h"
#include "render/Shader.h"

namespace sim {
class GravitySystem;
}

namespace render {

// The "spacetime" grid.
//
// IMPORTANT: this is a visual analogy, in the tradition of the rubber-sheet
// demonstration. It is not a general-relativistic calculation, and the
// displacement it draws does not feed back into the simulation in any way --
// bodies move under Newtonian gravity regardless of what this draws. The
// deformation is evaluated per-vertex on the GPU from the current body
// positions and masses. See docs/PHYSICS.md.
class GridRenderer {
public:
    bool initialize(const RenderSettings& settings);
    bool reloadShader() { return shader_.reload(); }

    // `gridCentre` is the point the sheet is laid out around, in world units,
    // absolute. It should be what the camera is looking at, not where it is.
    void draw(const sim::GravitySystem& system, const glm::dvec3& cameraPosition,
              const glm::dvec3& gridCentre, const glm::mat4& viewProjection,
              const RenderSettings& settings);

    // Number of wells the shader supports; must match MAX_WELLS in grid.vert.
    static constexpr int kMaxWells = 24;

    // The depth the grid would show at a point, evaluated on the CPU with the
    // same formula the shader uses. Exposed so the value can be unit tested and
    // so the UI can report it.
    static double wellDepthAt(const sim::GravitySystem& system, const glm::dvec3& gridPoint,
                              const RenderSettings& settings);

private:
    void rebuildMesh(int resolution, bool shaded);

    Shader shader_;
    Mesh mesh_;
    int builtResolution_ = 0;
    bool builtShaded_ = false;
};

}  // namespace render
