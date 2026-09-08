#pragma once

#include <string>
#include <vector>

#include <glm/mat4x4.hpp>

#include "engine/Camera.h"
#include "render/GridRenderer.h"
#include "render/LineRenderer.h"
#include "render/Mesh.h"
#include "render/RenderSettings.h"
#include "render/Shader.h"
#include "render/TrailRenderer.h"
#include "sim/CelestialBody.h"

namespace sim {
class GravitySystem;
}

namespace render {

// Everything drawn goes through here. The one invariant worth stating: every
// position handed to the GPU has already had the camera position subtracted in
// double precision and then been narrowed to float. Nothing downstream ever
// sees an absolute astronomical coordinate.
class Renderer {
public:
    bool initialize(const RenderSettings& settings);
    void reloadShaders();

    void beginFrame(int width, int height);
    void drawScene(const sim::GravitySystem& system, const engine::Camera& camera,
                   const RenderSettings& settings, float aspect,
                   sim::BodyId selected);

    // Radius the body is actually drawn at, in world units. Public because the
    // picker has to match what the viewer can see, not the physical radius.
    static double visualRadius(const sim::CelestialBody& body,
                               const RenderSettings& settings);

    // Chooses the power-law gain so that `referenceBody` -- or the scene's
    // physically largest body when that is empty -- draws at `targetRadius`
    // world units. Called when a scene loads; the result is stored in
    // RenderSettings so the viewer can then adjust it.
    static double gainForLargestRadius(const sim::GravitySystem& system,
                                       double metresPerUnit, double exponent,
                                       double targetRadius,
                                       const std::string& referenceBody = {});

    // Camera-relative position in world units.
    static glm::dvec3 relativePosition(const sim::CelestialBody& body,
                                       const glm::dvec3& cameraPosition,
                                       double metresPerUnit);

    // Picks the body whose drawn sphere a ray hits first. Returns
    // sim::kInvalidBodyId when nothing is hit. `rayDirection` must be
    // normalised and expressed in the same camera-relative space.
    static sim::BodyId pick(const sim::GravitySystem& system,
                            const glm::dvec3& cameraPosition,
                            const glm::vec3& rayDirection,
                            const RenderSettings& settings);

    // The world-unit point the spacetime sheet is laid out around: what the
    // camera is looking at, not where it is.
    static glm::dvec3 gridCentreFor(const engine::Camera& camera);

    LineRenderer& lines() { return lines_; }
    const glm::mat4& lastViewProjection() const { return viewProjection_; }

private:
    void drawBodies(const sim::GravitySystem& system, const engine::Camera& camera,
                    const RenderSettings& settings, sim::BodyId selected);
    void drawAnnotations(const sim::GravitySystem& system, const engine::Camera& camera,
                         const RenderSettings& settings, sim::BodyId selected);
    void rebuildSphere(const RenderSettings& settings);

    Shader bodyShader_;
    Mesh sphereMesh_;
    GridRenderer grid_;
    TrailRenderer trails_;
    LineRenderer lines_;

    glm::mat4 viewProjection_{1.0f};
    int sphereLatitude_ = 0;
    int sphereLongitude_ = 0;
};

}  // namespace render
