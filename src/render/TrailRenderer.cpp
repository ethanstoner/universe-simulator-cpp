#include "render/TrailRenderer.h"

#include <glad/gl.h>

#include <algorithm>

#include "sim/GravitySystem.h"

namespace render {

bool TrailRenderer::initialize() {
    return shader_.loadFromFiles("shaders/line.vert", "shaders/line.frag");
}

void TrailRenderer::draw(const sim::GravitySystem& system,
                         const glm::dvec3& cameraPosition, double metresPerUnit,
                         const glm::mat4& viewProjection, float opacity,
                         int maxSamples) {
    if (!shader_.valid() || opacity <= 0.0f || metresPerUnit <= 0.0) return;

    shader_.bind();
    shader_.setMat4("uViewProjection", viewProjection);
    shader_.setMat4("uModel", glm::mat4(1.0f));

    glDepthMask(GL_FALSE);  // trails should not occlude one another

    // Trails are stored in the frame named by trailReference, so drawing them
    // means adding that body's *current* position back. The result is the
    // relative path rigidly attached to where the reference body is now.
    sim::Vec3 frameOrigin(0.0);
    if (system.settings().trailReference != sim::kInvalidBodyId) {
        if (const sim::CelestialBody* reference =
                system.find(system.settings().trailReference)) {
            frameOrigin = reference->position;
        }
    }

    for (const sim::CelestialBody& body : system.bodies()) {
        if (!body.showTrail || body.trail.size() < 2) continue;

        const std::size_t total = body.trail.size();
        const std::size_t wanted =
            std::min<std::size_t>(total, static_cast<std::size_t>(std::max(maxSamples, 2)));
        const std::size_t skip = total - wanted;

        scratch_.clear();
        scratch_.reserve(wanted);

        std::size_t index = 0;
        for (const sim::Vec3& point : body.trail) {
            if (index++ < skip) continue;
            // Camera-relative in double, then narrowed to float. Doing the
            // subtraction after the cast would quantise a 1e11 m position to
            // ~10 000 km and make the trail visibly staircase.
            const glm::dvec3 relative =
                (point + frameOrigin) / metresPerUnit - cameraPosition;
            const float fade = static_cast<float>(scratch_.size() + 1) /
                               static_cast<float>(wanted);
            scratch_.push_back({glm::vec3(relative), glm::vec3(fade, 0.0f, 0.0f)});
        }
        if (scratch_.size() < 2) continue;

        mesh_.updateVertices(scratch_);
        shader_.setVec4("uColor", glm::vec4(body.color, opacity));
        mesh_.drawRange(DrawMode::LineStrip, scratch_.size());
    }

    glDepthMask(GL_TRUE);
}

}  // namespace render
