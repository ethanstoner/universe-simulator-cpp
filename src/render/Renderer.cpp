#include "render/Renderer.h"

#include <glad/gl.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

#include "render/MeshFactory.h"
#include "sim/GravitySystem.h"
#include "sim/OrbitMath.h"

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

glm::dvec3 Renderer::relativePosition(const sim::CelestialBody& body,
                                      const glm::dvec3& cameraPosition,
                                      double metresPerUnit) {
    if (metresPerUnit <= 0.0) return glm::dvec3(0.0);
    // The subtraction happens in double, before any narrowing. This is the
    // floating-origin step; doing it after a cast to float would quantise a
    // 1.5e11 m coordinate to steps of about 16 000 m.
    return glm::dvec3(body.position) / metresPerUnit - cameraPosition;
}

double Renderer::visualRadius(const sim::CelestialBody& body,
                              const RenderSettings& settings) {
    if (settings.metresPerUnit <= 0.0) return settings.minVisualRadius;

    // The body's real radius, expressed in world units. This is the only place
    // the physical radius is used for anything visual, and the result is used
    // for nothing physical.
    const double trueRadius = body.radius / settings.metresPerUnit;
    if (settings.trueScale) {
        return std::min(trueRadius, settings.maxVisualRadius);
    }
    if (trueRadius <= 0.0) return settings.minVisualRadius;

    const double scaled =
        settings.bodyVisualGain * std::pow(trueRadius, settings.bodyVisualExponent);
    return std::clamp(scaled, settings.minVisualRadius, settings.maxVisualRadius);
}

double Renderer::gainForLargestRadius(const sim::GravitySystem& system,
                                      double metresPerUnit, double exponent,
                                      double targetRadius,
                                      const std::string& referenceBody) {
    if (metresPerUnit <= 0.0 || exponent <= 0.0) return 1.0;

    double largest = 0.0;
    if (!referenceBody.empty()) {
        for (const sim::CelestialBody& body : system.bodies()) {
            if (body.name == referenceBody) {
                largest = body.radius / metresPerUnit;
                break;
            }
        }
    }
    if (largest <= 0.0) {
        for (const sim::CelestialBody& body : system.bodies()) {
            largest = std::max(largest, body.radius / metresPerUnit);
        }
    }
    if (largest <= 0.0) return 1.0;
    // Solve gain * largest^exponent = targetRadius, so that whatever the scene
    // is, its biggest body draws at a sensible fraction of the view and
    // everything else falls into place beneath it.
    return targetRadius / std::pow(largest, exponent);
}

sim::BodyId Renderer::pick(const sim::GravitySystem& system,
                           const glm::dvec3& cameraPosition,
                           const glm::vec3& rayDirection,
                           const RenderSettings& settings) {
    sim::BodyId best = sim::kInvalidBodyId;
    double bestDistance = 1e30;

    for (const sim::CelestialBody& body : system.bodies()) {
        const glm::dvec3 centre =
            relativePosition(body, cameraPosition, settings.metresPerUnit);
        // Picking uses the *drawn* radius, with a floor in case a body is
        // rendered smaller than a comfortable click target.
        const double radius = std::max(visualRadius(body, settings), 0.05);

        // Ray-sphere intersection with the ray origin at the camera (the
        // origin of this space), so b = -2 * dot(dir, centre).
        const glm::dvec3 direction(rayDirection);
        const double projection = glm::dot(direction, centre);
        if (projection <= 0.0) continue;  // behind the camera

        const double perpendicularSq = glm::dot(centre, centre) - projection * projection;
        if (perpendicularSq > radius * radius) continue;

        const double halfChord = std::sqrt(radius * radius - perpendicularSq);
        const double hit = projection - halfChord;
        if (hit > 0.0 && hit < bestDistance) {
            bestDistance = hit;
            best = body.id;
        }
    }
    return best;
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
    rebuildSphere(settings);
    return ok;
}

void Renderer::reloadShaders() {
    bodyShader_.reload();
    grid_.reloadShader();
    trails_.reloadShader();
    lines_.reloadShader();
}

void Renderer::beginFrame(int width, int height) {
    glViewport(0, 0, width, height);
    glClearColor(0.015f, 0.017f, 0.032f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::drawScene(const sim::GravitySystem& system, const engine::Camera& camera,
                         const RenderSettings& settings, float aspect,
                         sim::BodyId selected) {
    viewProjection_ = camera.viewProjection(aspect);

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
