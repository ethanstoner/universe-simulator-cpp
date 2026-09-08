#include "sim/ViewMath.h"

#include <algorithm>
#include <cmath>

#include "sim/GravitySystem.h"

namespace sim {

Vec3 relativePosition(const CelestialBody& body, const Vec3& cameraPosition,
                      double metresPerUnit) {
    if (metresPerUnit <= 0.0) return Vec3(0.0);
    return body.position / metresPerUnit - cameraPosition;
}

double visualRadius(const CelestialBody& body, const ViewScale& scale) {
    if (scale.metresPerUnit <= 0.0) return scale.minRadius;

    const double trueRadius = body.radius / scale.metresPerUnit;
    if (scale.trueScale) return std::min(trueRadius, scale.maxRadius);
    if (trueRadius <= 0.0) return scale.minRadius;

    const double scaled = scale.gain * std::pow(trueRadius, scale.exponent);
    return std::clamp(scaled, scale.minRadius, scale.maxRadius);
}

double solveVisualGain(const GravitySystem& system, double metresPerUnit,
                       double exponent, double targetRadius,
                       const std::string& referenceBody) {
    if (metresPerUnit <= 0.0 || exponent <= 0.0) return 1.0;

    double reference = 0.0;
    if (!referenceBody.empty()) {
        for (const CelestialBody& body : system.bodies()) {
            if (body.name == referenceBody) {
                reference = body.radius / metresPerUnit;
                break;
            }
        }
    }
    if (reference <= 0.0) {
        for (const CelestialBody& body : system.bodies()) {
            reference = std::max(reference, body.radius / metresPerUnit);
        }
    }
    if (reference <= 0.0) return 1.0;

    // Solve gain * reference^exponent = targetRadius.
    return targetRadius / std::pow(reference, exponent);
}

BodyId pickBody(const GravitySystem& system, const Vec3& cameraPosition,
                const Vec3& direction, const ViewScale& scale, double minPickRadius) {
    BodyId best = kInvalidBodyId;
    double bestDistance = 1e300;

    for (const CelestialBody& body : system.bodies()) {
        const Vec3 centre = relativePosition(body, cameraPosition, scale.metresPerUnit);
        // Picking uses the DRAWN radius, not the physical one: the user can only
        // click what they can see.
        const double radius = std::max(visualRadius(body, scale), minPickRadius);

        // Ray-sphere intersection with the ray origin at the camera, which is
        // the origin of this space, so the projection of the centre onto the
        // ray is simply dot(direction, centre).
        const double projection = glm::dot(direction, centre);
        if (projection <= 0.0) continue;  // behind the camera

        const double perpendicularSq = lengthSquared(centre) - projection * projection;
        if (perpendicularSq > radius * radius) continue;

        const double halfChord = std::sqrt(radius * radius - perpendicularSq);
        // Prefer the near intersection; if the camera is inside the sphere the
        // near root is negative and the body still counts as hit.
        double hit = projection - halfChord;
        if (hit < 0.0) hit = projection + halfChord;
        if (hit > 0.0 && hit < bestDistance) {
            bestDistance = hit;
            best = body.id;
        }
    }
    return best;
}

}  // namespace sim
