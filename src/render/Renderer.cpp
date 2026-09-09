#include "render/Renderer.h"

#include <glad/gl.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

#include "render/MeshFactory.h"
#include "sim/GravitySystem.h"
#include "sim/OrbitMath.h"
#include "sim/ViewMath.h"

namespace render {
namespace {

// Stars in the scene, used as light sources. Capped to match MAX_LIGHTS in
// body.frag.
constexpr int kMaxLights = 4;

struct Light {
    glm::vec3 position;
    glm::vec3 color;
};

std::vector<Light> gatherLights(const sim::GravitySystem& system,
                                const glm::dvec3& cameraPosition,
                                const RenderSettings& settings) {
    std::vector<const sim::CelestialBody*> stars;
    for (const sim::CelestialBody& body : system.bodies()) {
        if (body.emissive) stars.push_back(&body);
    }
    // Brightest (heaviest) first, so a binary keeps both stars but a crowd of
    // spawned suns does not push the real one out.
    std::sort(stars.begin(), stars.end(),
              [](const sim::CelestialBody* a, const sim::CelestialBody* b) {
                  return a->mass > b->mass;
              });
    if (stars.size() > kMaxLights) stars.resize(kMaxLights);

    std::vector<Light> lights;
    for (const sim::CelestialBody* star : stars) {
        const glm::dvec3 relative =
            Renderer::relativePosition(*star, cameraPosition, settings.metresPerUnit);
        lights.push_back({glm::vec3(relative), star->color * settings.starBrightness});
    }

    if (lights.empty()) {
        // A scene with no star still has to be visible, so fall back to a fixed
        // key light attached to the camera.
        lights.push_back({glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.85f)});
    }
    return lights;
}

}  // namespace

render::ViewScale Renderer::toViewScale(const RenderSettings& settings) {
    sim::ViewScale scale;
    scale.metresPerUnit = settings.metresPerUnit;
    scale.gain = settings.bodyVisualGain;
    scale.exponent = settings.bodyVisualExponent;
    scale.minRadius = settings.minVisualRadius;
    scale.maxRadius = settings.maxVisualRadius;
    scale.trueScale = settings.trueScale;
    return scale;
}

glm::dvec3 Renderer::relativePosition(const sim::CelestialBody& body,
                                      const glm::dvec3& cameraPosition,
                                      double metresPerUnit) {
    return sim::relativePosition(body, cameraPosition, metresPerUnit);
}

double Renderer::visualRadius(const sim::CelestialBody& body,
                              const RenderSettings& settings) {
    return sim::visualRadius(body, toViewScale(settings));
}

double Renderer::gainForLargestRadius(const sim::GravitySystem& system,
                                      double metresPerUnit, double exponent,
                                      double targetRadius,
                                      const std::string& referenceBody) {
    return sim::solveVisualGain(system, metresPerUnit, exponent, targetRadius,
                                referenceBody);
}

sim::BodyId Renderer::pick(const sim::GravitySystem& system,
                           const glm::dvec3& cameraPosition,
                           const glm::vec3& rayDirection,
                           const RenderSettings& settings) {
    return sim::pickBody(system, cameraPosition, glm::dvec3(rayDirection),
                         toViewScale(settings));
}

glm::dvec3 Renderer::gridCentreFor(const engine::Camera& camera) {
    // In orbit mode the look-at point is exactly the orbit target. In free
    // flight, project the view ray onto the y = 0 plane; if the camera is
    // looking level or upwards there is no intersection, so fall back to a
    // point straight ahead at the current fly distance.
    if (camera.mode() == engine::CameraMode::Orbit) {
        return camera.orbitTarget();
    }
    const glm::dvec3 position = camera.position();
    const glm::dvec3 forward(camera.forward());
    if (forward.y < -1e-3) {
        const double t = -position.y / forward.y;
        return position + forward * t;
    }
    return position + forward * static_cast<double>(camera.moveSpeed);
}

void Renderer::rebuildSphere(const RenderSettings& settings) {
    sphereMesh_ = uploadMesh(makeUvSphere(settings.sphereLatitudeSegments,
                                          settings.sphereLongitudeSegments));
    sphereLatitude_ = settings.sphereLatitudeSegments;
    sphereLongitude_ = settings.sphereLongitudeSegments;
}

bool Renderer::initialize(const RenderSettings& settings) {
    bool ok = bodyShader_.loadFromFiles("shaders/body.vert", "shaders/body.frag");
    ok = grid_.initialize(settings) && ok;
    ok = trails_.initialize() && ok;
    ok = lines_.initialize() && ok;
    ok = starfield_.initialize() && ok;
    // Post-processing failing is not fatal: the renderer falls back to drawing
    // straight to the default framebuffer, losing bloom but not the scene.
    if (!post_.initialize()) {
        std::fprintf(stderr, "[render] post-processing unavailable: %s\n",
                     post_.lastError().c_str());
    }
    rebuildSphere(settings);
    return ok;
}

void Renderer::reloadShaders() {
    bodyShader_.reload();
    grid_.reloadShader();
    trails_.reloadShader();
    lines_.reloadShader();
    starfield_.reloadShader();
    post_.reloadShaders();
}

void Renderer::beginFrame(int width, int height, int samples,
                          const RenderSettings& settings) {
    // A near-black background rather than pure black: the grid and trails read
    // better against a very slightly blue field.
    const glm::vec3 clearColor(0.008f, 0.009f, 0.018f);

    usingPost_ = false;
    if (settings.postProcessEnabled) {
        usingPost_ = post_.resize(width, height, samples) && post_.begin(clearColor);
    }
    if (!usingPost_) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::endFrame(const RenderSettings& settings) {
    if (usingPost_) {
        post_.end(settings);
        usingPost_ = false;
    }
}

void Renderer::drawScene(const sim::GravitySystem& system, const engine::Camera& camera,
                         const RenderSettings& settings, float aspect,
                         sim::BodyId selected) {
    viewProjection_ = camera.viewProjection(aspect);

    // Stars first: they are the backdrop, and they write no depth.
    starfield_.draw(viewProjection_, settings);

    if (settings.gridResolution != 0 && settings.showGrid) {
        grid_.draw(system, camera.position(), gridCentreFor(camera), viewProjection_,
                   settings);
    }
    if (settings.showBodies) {
        drawBodies(system, camera, settings, selected);
    }
    if (settings.showTrails) {
        trails_.draw(system, camera.position(), settings.metresPerUnit, viewProjection_,
                     settings.trailOpacity, settings.trailMaxSamples);
    }
    drawAnnotations(system, camera, settings, selected);
}

void Renderer::drawTrajectory(const sim::Trajectory& trajectory,
                              const sim::GravitySystem& system,
                              const glm::dvec3& cameraPosition,
                              const RenderSettings& settings) {
    if (trajectory.points.size() < 2 || settings.metresPerUnit <= 0.0) return;

    // Predictions stored relative to a body are drawn attached to where that
    // body is now, exactly as trails are.
    sim::Vec3 frameOrigin(0.0);
    if (system.settings().trailReference != sim::kInvalidBodyId) {
        if (const sim::CelestialBody* reference =
                system.find(system.settings().trailReference)) {
            frameOrigin = reference->position;
        }
    }

    lines_.begin();
    const std::size_t count = trajectory.points.size();
    for (std::size_t i = 0; i + 1 < count; ++i) {
        const glm::dvec3 a =
            (trajectory.points[i] + frameOrigin) / settings.metresPerUnit - cameraPosition;
        const glm::dvec3 b =
            (trajectory.points[i + 1] + frameOrigin) / settings.metresPerUnit -
            cameraPosition;
        // Fades along its length so the direction of travel is obvious and the
        // forecast is visually distinct from the solid trail behind the body.
        // Only a gentle fade: at 0.75 the far half of a full-orbit forecast was
        // so faint that the prediction looked truncated.
        const float t = static_cast<float>(i) / static_cast<float>(count - 1);
        const float alpha = 0.95f * (1.0f - t * 0.45f);
        lines_.addSegment(glm::vec3(a), glm::vec3(b),
                          glm::vec4(0.45f, 0.95f, 0.75f, alpha));
    }
    glDepthMask(GL_FALSE);
    lines_.flush(viewProjection_);
    glDepthMask(GL_TRUE);
}

void Renderer::drawBodies(const sim::GravitySystem& system, const engine::Camera& camera,
                          const RenderSettings& settings, sim::BodyId selected) {
    if (!bodyShader_.valid()) return;
    if (settings.sphereLatitudeSegments != sphereLatitude_ ||
        settings.sphereLongitudeSegments != sphereLongitude_) {
        rebuildSphere(settings);
    }

    const std::vector<Light> lights = gatherLights(system, camera.position(), settings);

    bodyShader_.bind();
    bodyShader_.setMat4("uViewProjection", viewProjection_);
    bodyShader_.setFloat("uAmbient", settings.ambient);
    bodyShader_.setInt("uLightCount", static_cast<int>(lights.size()));
    for (std::size_t i = 0; i < lights.size(); ++i) {
        char name[40];
        std::snprintf(name, sizeof(name), "uLightPositions[%zu]", i);
        bodyShader_.setVec3(name, lights[i].position);
        std::snprintf(name, sizeof(name), "uLightColors[%zu]", i);
        bodyShader_.setVec3(name, lights[i].color);
    }

    if (settings.wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    for (const sim::CelestialBody& body : system.bodies()) {
        const glm::dvec3 relative =
            relativePosition(body, camera.position(), settings.metresPerUnit);
        const double radius = visualRadius(body, settings);

        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(relative));
        model = glm::scale(model, glm::vec3(static_cast<float>(radius)));

        bodyShader_.setMat4("uModel", model);
        // The scale is uniform, so this reduces to the identity, but computing
        // it properly keeps the shader correct if a non-uniform scale (an
        // oblate planet, say) is ever introduced.
        bodyShader_.setMat3("uNormalMatrix", glm::inverseTranspose(glm::mat3(model)));
        bodyShader_.setVec3("uBaseColor", body.color);
        bodyShader_.setFloat("uEmissive", body.emissive ? 1.0f : 0.0f);
        bodyShader_.setFloat("uSelected", body.id == selected ? 1.0f : 0.0f);

        sphereMesh_.draw();
    }

    if (settings.wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void Renderer::drawAnnotations(const sim::GravitySystem& system,
                               const engine::Camera& camera,
                               const RenderSettings& settings, sim::BodyId selected) {
    const bool wantsAnything = settings.showVelocityVectors ||
                               settings.showAccelerationVectors ||
                               settings.showSchwarzschildRadius || settings.showBoundsBox;
    if (!wantsAnything) return;

    lines_.begin();
    const glm::vec3 cameraRight = camera.right();
    const glm::vec3 cameraUp = camera.up();

    // Arrows are scaled so the fastest body in the scene gets a fixed on-screen
    // length. Drawing them at physical scale would make every arrow either
    // invisible or kilometres long depending on the preset.
    double fastest = 0.0;
    double strongest = 0.0;
    for (const sim::CelestialBody& body : system.bodies()) {
        fastest = std::max(fastest, glm::length(body.velocity));
        strongest = std::max(strongest, glm::length(body.acceleration));
    }
    const double arrowLength = settings.gridExtent * 0.08;

    for (const sim::CelestialBody& body : system.bodies()) {
        const glm::vec3 centre = glm::vec3(
            relativePosition(body, camera.position(), settings.metresPerUnit));

        if (settings.showVelocityVectors && fastest > 0.0) {
            const glm::vec3 direction =
                glm::vec3(glm::normalize(body.velocity)) *
                static_cast<float>(arrowLength * glm::length(body.velocity) / fastest);
            lines_.addArrow(centre, centre + direction, glm::vec4(0.4f, 0.9f, 1.0f, 0.85f),
                            cameraRight, cameraUp);
        }
        if (settings.showAccelerationVectors && strongest > 0.0 &&
            glm::length(body.acceleration) > 0.0) {
            const glm::vec3 direction =
                glm::vec3(glm::normalize(body.acceleration)) *
                static_cast<float>(arrowLength * 0.8 *
                                   glm::length(body.acceleration) / strongest);
            lines_.addArrow(centre, centre + direction, glm::vec4(1.0f, 0.6f, 0.35f, 0.85f),
                            cameraRight, cameraUp);
        }
        if (settings.showSchwarzschildRadius && body.mass > 0.0) {
            // Drawn at true scale in world units, with no exaggeration: for the
            // Sun this is 2.95 km against a 7e5 km radius, so it is invisible
            // unless the body really is compact. That is the honest result and
            // the UI says so.
            const double rs = sim::schwarzschildRadius(body.mass) / settings.metresPerUnit;
            if (rs > 1e-4) {
                const glm::vec4 color(1.0f, 0.25f, 0.35f, 0.9f);
                lines_.addRing(centre, rs, glm::vec3(0.0f, 1.0f, 0.0f), color);
                lines_.addRing(centre, rs, glm::vec3(1.0f, 0.0f, 0.0f), color);
                lines_.addRing(centre, rs, glm::vec3(0.0f, 0.0f, 1.0f), color);
            }
        }
        if (body.id == selected) {
            const double radius = visualRadius(body, settings) * 1.6;
            const glm::vec4 color(0.95f, 0.95f, 0.45f, 0.9f);
            lines_.addRing(centre, radius, cameraUp, color, 48);
            lines_.addRing(centre, radius, cameraRight, color, 48);
        }
    }

    if (settings.showBoundsBox && system.settings().bounds.enabled) {
        const sim::Bounds& bounds = system.settings().bounds;
        const glm::dvec3 min =
            glm::dvec3(bounds.min) / settings.metresPerUnit - camera.position();
        const glm::dvec3 max =
            glm::dvec3(bounds.max) / settings.metresPerUnit - camera.position();
        lines_.addBox(glm::vec3(min), glm::vec3(max), glm::vec4(0.3f, 0.4f, 0.55f, 0.8f));
    }

    lines_.flush(viewProjection_);
}

}  // namespace render
