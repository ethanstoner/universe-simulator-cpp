#pragma once

#include <string>

#include <glm/vec3.hpp>

#include "sim/CelestialBody.h"
#include "sim/Vec.h"

namespace sim {

class GravitySystem;

// The pure maths behind how the scene is *viewed*: metres-to-world-units,
// exaggerated body radii, and ray picking. It lives in sim/ rather than
// render/ only so it can be unit tested without an OpenGL context -- nothing
// here is read by the physics, and the renderer is its only caller.
struct ViewScale {
    double metresPerUnit = 1.0;
    // drawnRadius = gain * (trueRadius / metresPerUnit) ^ exponent.
    // A plain multiplier (exponent 1) is unusable at astronomical scale: the
    // Sun is 109 Earth radii, so anything that makes the Earth visible makes
    // the Sun wider than the Earth's orbit.
    double gain = 1.0;
    double exponent = 0.35;
    double minRadius = 0.02;
    double maxRadius = 40.0;
    bool trueScale = false;
};

// Camera-relative position in world units. The subtraction is done in double
// before any narrowing to float, which is the whole point of the
// floating-origin scheme.
Vec3 relativePosition(const CelestialBody& body, const Vec3& cameraPosition,
                      double metresPerUnit);

// The radius the body is actually drawn at, in world units.
double visualRadius(const CelestialBody& body, const ViewScale& scale);

// Solves `gain` so that `referenceBody` -- or the physically largest body when
// that name is empty or not found -- draws at `targetRadius`.
double solveVisualGain(const GravitySystem& system, double metresPerUnit,
                       double exponent, double targetRadius,
                       const std::string& referenceBody = {});

// Returns the body whose drawn sphere the ray hits first, or kInvalidBodyId.
// The ray starts at the camera and `direction` must be normalised; both are in
// camera-relative world-unit space. `minPickRadius` widens the target so a body
// drawn a few pixels across is still clickable.
BodyId pickBody(const GravitySystem& system, const Vec3& cameraPosition,
                const Vec3& direction, const ViewScale& scale,
                double minPickRadius = 0.05);

}  // namespace sim
