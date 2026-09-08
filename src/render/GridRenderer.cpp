#include "render/GridRenderer.h"

#include <glad/gl.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "render/MeshFactory.h"
#include "sim/GravitySystem.h"

namespace render {
namespace {

// Depth of one well, matching wellDepth() in shaders/grid.vert exactly.
double wellDepth(const glm::dvec3& gridPoint, const glm::dvec3& wellPosition,
                 double strength, double radius) {
    const double dx = gridPoint.x - wellPosition.x;
    const double dz = gridPoint.z - wellPosition.z;
    const double softened = std::sqrt(dx * dx + dz * dz + radius * radius);
    return -strength / softened;
}

// The strength handed to the shader is GM expressed in world units, normalised
// so that the deepest well in the scene produces a well-proportioned funnel
// regardless of whether the scene is a solar system or a bouncing ball.
struct WellSet {
    std::vector<glm::vec3> positions;
    std::vector<float> strengths;
    std::vector<float> radii;
};

WellSet gatherWells(const sim::GravitySystem& system, const glm::dvec3& cameraPosition,
                    const glm::dvec3& gridCentre, const RenderSettings& settings) {
    WellSet wells;
    if (settings.metresPerUnit <= 0.0) return wells;

    // The patch is centred under the camera, so "in view" means within a few
    // grid extents of it in the XZ plane.
    const double cullRadius = settings.gridExtent * 3.0;

    struct Candidate {
        const sim::CelestialBody* body;
        glm::dvec3 relative;   // camera-relative, world units
        double planarDistance; // from the patch centre, in the grid plane
    };

    std::vector<Candidate> candidates;
    candidates.reserve(system.bodies().size());
    for (const sim::CelestialBody& body : system.bodies()) {
        if (body.mass <= 0.0) continue;
        const glm::dvec3 relative =
            glm::dvec3(body.position) / settings.metresPerUnit - cameraPosition;
        // Culling is judged against the patch centre, which is where the
        // camera is looking rather than where the camera is.
        const double dx = glm::dvec3(body.position).x / settings.metresPerUnit - gridCentre.x;
        const double dz = glm::dvec3(body.position).z / settings.metresPerUnit - gridCentre.z;
        const double planar = std::sqrt(dx * dx + dz * dz);
        candidates.push_back({&body, relative, planar});
    }
    if (candidates.empty()) return wells;

    // A body far outside the patch contributes an almost constant offset
    // rather than a visible well, and including it would push the whole sheet
    // into the saturation clamp. In the Earth-Moon preset the Sun is 2 992
    // units away against a 26 unit patch, which flattened everything.
    std::vector<Candidate> inView;
    for (const Candidate& candidate : candidates) {
        if (candidate.planarDistance <= cullRadius) inView.push_back(candidate);
    }
    // If nothing is near, keep the heaviest so the sheet is not perfectly flat.
    if (inView.empty()) {
        const auto heaviest = std::max_element(
            candidates.begin(), candidates.end(),
            [](const Candidate& a, const Candidate& b) { return a.body->mass < b.body->mass; });
        inView.push_back(*heaviest);
    }

    std::sort(inView.begin(), inView.end(), [](const Candidate& a, const Candidate& b) {
        return a.body->mass > b.body->mass;
    });
    if (inView.size() > GridRenderer::kMaxWells) inView.resize(GridRenderer::kMaxWells);

    const double softeningRadius =
        std::max(settings.gridExtent * settings.gridWellSoftening, 1e-6);

    // Normalise against the heaviest body *that is actually on the patch*, so
    // gridStrength = 1 gives a readable funnel in every preset. This is a
    // presentation choice and is documented as one: the shape of each well is
    // the softened Newtonian potential, but the vertical gain is arbitrary.
    const double heaviestInView = inView.front().body->mass;
    const double gain = settings.gridMaxDepth * softeningRadius / heaviestInView;

    for (const Candidate& candidate : inView) {
        wells.positions.push_back(glm::vec3(candidate.relative));
        wells.strengths.push_back(static_cast<float>(candidate.body->mass * gain));
        wells.radii.push_back(static_cast<float>(softeningRadius));
    }
    return wells;
}

}  // namespace

double GridRenderer::wellDepthAt(const sim::GravitySystem& system,
                                 const glm::dvec3& gridPoint,
                                 const RenderSettings& settings) {
    // Evaluated in the same camera-relative space the shader uses, with the
    // camera at the origin, so callers pass an already-relative point.
    const WellSet wells = gatherWells(system, glm::dvec3(0.0), glm::dvec3(0.0), settings);
    double depth = 0.0;
    for (std::size_t i = 0; i < wells.positions.size(); ++i) {
        depth += wellDepth(gridPoint, glm::dvec3(wells.positions[i]), wells.strengths[i],
                           wells.radii[i]);
    }
    depth *= settings.gridStrength;
    const double maxDepth = std::max(settings.gridMaxDepth, 1e-6);
    return -maxDepth * (1.0 - std::exp(depth / maxDepth));
}

void GridRenderer::rebuildMesh(int resolution, bool shaded) {
    mesh_ = uploadMesh(shaded ? makeGridSurface(resolution) : makeGridLines(resolution));
    builtResolution_ = resolution;
    builtShaded_ = shaded;
}

bool GridRenderer::initialize(const RenderSettings& settings) {
    if (!shader_.loadFromFiles("shaders/grid.vert", "shaders/grid.frag")) return false;
    rebuildMesh(settings.gridResolution, settings.gridShaded);
    return true;
}

void GridRenderer::draw(const sim::GravitySystem& system, const glm::dvec3& cameraPosition,
                        const glm::dvec3& gridCentre, const glm::mat4& viewProjection,
                        const RenderSettings& settings) {
    if (!shader_.valid() || !settings.showGrid) return;

    if (settings.gridResolution != builtResolution_ || settings.gridShaded != builtShaded_) {
        rebuildMesh(settings.gridResolution, settings.gridShaded);
    }

    const WellSet wells = gatherWells(system, cameraPosition, gridCentre, settings);

    shader_.bind();
    shader_.setMat4("uViewProjection", viewProjection);
    shader_.setInt("uWellCount", static_cast<int>(wells.positions.size()));
    if (!wells.positions.empty()) {
        // Uniform arrays are set element-by-element rather than with a single
        // glUniform3fv call, so the code does not depend on the driver laying
        // the array out contiguously.
        for (std::size_t i = 0; i < wells.positions.size(); ++i) {
            char name[48];
            std::snprintf(name, sizeof(name), "uWellPositions[%zu]", i);
            shader_.setVec3(name, wells.positions[i]);
            std::snprintf(name, sizeof(name), "uWellStrength[%zu]", i);
            shader_.setFloat(name, wells.strengths[i]);
            std::snprintf(name, sizeof(name), "uWellRadius[%zu]", i);
            shader_.setFloat(name, wells.radii[i]);
        }
    }

    // The patch is centred under the point the camera is LOOKING AT, not under
    // the camera itself. Centring it under the camera puts the scene a full
    // orbit-distance from the middle of the sheet -- at the binary preset's 70
    // unit camera distance against a 55 unit patch, the stars fell off the edge
    // entirely and their wells were invisible.
    glm::dvec3 origin(0.0);
    if (settings.gridFollowsCamera) {
        origin = glm::dvec3(gridCentre.x - cameraPosition.x, -cameraPosition.y,
                            gridCentre.z - cameraPosition.z);
    } else {
        origin = -cameraPosition;  // patch pinned to the world origin
    }

    shader_.setVec3("uGridOrigin", glm::vec3(origin));
    shader_.setFloat("uGridExtent", static_cast<float>(settings.gridExtent));
    shader_.setFloat("uMaxDepth", static_cast<float>(settings.gridMaxDepth));
    shader_.setFloat("uStrength", settings.gridStrength);
    shader_.setVec3("uShallowColor", glm::vec3(0.24f, 0.42f, 0.62f));
    shader_.setVec3("uDeepColor", glm::vec3(0.62f, 0.42f, 0.92f));
    shader_.setFloat("uOpacity", settings.gridOpacity);
    // The far corner of the patch, as seen from the camera. Anything closer
    // than 72% of this stays at full opacity, so the sheet is always visible
    // however far back the camera is pulled.
    const double farCorner =
        glm::length(origin) + settings.gridExtent * 1.4142 + 1.0;
    shader_.setFloat("uFadeDistance", static_cast<float>(farCorner));

    // Blended and depth-write-disabled: the grid is an overlay, and letting it
    // write depth would make bodies behind it vanish.
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    mesh_.draw(builtShaded_ ? DrawMode::Triangles : DrawMode::Lines);
    glDepthMask(GL_TRUE);
}

}  // namespace render
