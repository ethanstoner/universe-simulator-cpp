#include "sim/OrbitMath.h"

#include <algorithm>
#include <cmath>

namespace sim {
namespace {
constexpr double kPi = 3.14159265358979323846;
}

Vec3 progradeTangent(double phaseRadians) {
    return Vec3(std::sin(phaseRadians), 0.0, -std::cos(phaseRadians));
}

double orbitSense(const Vec3& relativePosition, const Vec3& relativeVelocity) {
    const double h = glm::cross(relativePosition, relativeVelocity).y;
    return h < 0.0 ? -1.0 : 1.0;
}

double circularOrbitSpeed(double centralMass, double radius, double G) {
    if (radius <= 0.0 || centralMass <= 0.0) return 0.0;
    return std::sqrt(G * centralMass / radius);
}

double visVivaSpeed(double centralMass, double radius, double semiMajorAxis, double G) {
    if (radius <= 0.0 || centralMass <= 0.0 || semiMajorAxis == 0.0) return 0.0;
    const double vSquared = G * centralMass * (2.0 / radius - 1.0 / semiMajorAxis);
    return vSquared > 0.0 ? std::sqrt(vSquared) : 0.0;
}

double escapeSpeed(double centralMass, double radius, double G) {
    if (radius <= 0.0 || centralMass <= 0.0) return 0.0;
    return std::sqrt(2.0 * G * centralMass / radius);
}

double orbitalPeriod(double centralMass, double semiMajorAxis, double G) {
    if (semiMajorAxis <= 0.0 || centralMass <= 0.0) return 0.0;
    return 2.0 * kPi * std::sqrt(semiMajorAxis * semiMajorAxis * semiMajorAxis /
                                 (G * centralMass));
}

double schwarzschildRadius(double mass, double G, double c) {
    if (mass <= 0.0) return 0.0;
    return 2.0 * G * mass / (c * c);
}

double perihelionDistance(double semiMajorAxis, double eccentricity) {
    return semiMajorAxis * (1.0 - eccentricity);
}

double perihelionSpeed(double centralMass, double semiMajorAxis, double eccentricity,
                       double G) {
    if (semiMajorAxis <= 0.0 || centralMass <= 0.0) return 0.0;
    if (eccentricity < 0.0 || eccentricity >= 1.0) return 0.0;
    // Vis-viva at r = a(1-e): v^2 = GM (2/r - 1/a) = GM/a * (1+e)/(1-e).
    return std::sqrt(G * centralMass / semiMajorAxis * (1.0 + eccentricity) /
                     (1.0 - eccentricity));
}

double relativisticPerihelionAdvance(double centralMass, double semiMajorAxis,
                                     double eccentricity, double G, double c) {
    if (semiMajorAxis <= 0.0 || centralMass <= 0.0) return 0.0;
    const double denominator = c * c * semiMajorAxis * (1.0 - eccentricity * eccentricity);
    if (denominator <= 0.0) return 0.0;
    return 6.0 * kPi * G * centralMass / denominator;
}

double periapsisAngle(const Vec3& relativePosition, const Vec3& relativeVelocity,
                      double mu) {
    const double r = glm::length(relativePosition);
    if (r <= 0.0 || mu <= 0.0) return 0.0;
    const Vec3 angularMomentum = glm::cross(relativePosition, relativeVelocity);
    // e = (v x h)/mu - r_hat, which points from the focus towards periapsis.
    const Vec3 eccentricityVector =
        glm::cross(relativeVelocity, angularMomentum) / mu - relativePosition / r;
    // The presets lay orbits out in the XZ plane, so that is the plane to
    // measure the angle in.
    return std::atan2(eccentricityVector.z, eccentricityVector.x);
}

OrbitalElements computeOrbitalElements(const Vec3& relativePosition,
                                       const Vec3& relativeVelocity, double mu) {
    OrbitalElements elements;
    const double r = glm::length(relativePosition);
    if (r <= 0.0 || mu <= 0.0) return elements;

    const double vSquared = lengthSquared(relativeVelocity);

    // Specific orbital energy: eps = v^2/2 - mu/r.
    elements.specificEnergy = 0.5 * vSquared - mu / r;

    const Vec3 angularMomentum = glm::cross(relativePosition, relativeVelocity);
    const double h = glm::length(angularMomentum);

    // Eccentricity vector: e = (v x h)/mu - r_hat.
    const Vec3 eccentricityVector =
        glm::cross(relativeVelocity, angularMomentum) / mu - relativePosition / r;
    elements.eccentricity = glm::length(eccentricityVector);

    if (elements.specificEnergy != 0.0) {
        elements.semiMajorAxis = -mu / (2.0 * elements.specificEnergy);
    }
    elements.bound = elements.specificEnergy < 0.0;

    if (h > 0.0) {
        // Periapsis and apoapsis from the semi-latus rectum, which stays finite
        // for parabolic orbits where the semi-major axis does not.
        const double semiLatusRectum = h * h / mu;
        elements.periapsis = semiLatusRectum / (1.0 + elements.eccentricity);
        if (elements.bound && elements.eccentricity < 1.0) {
            elements.apoapsis = semiLatusRectum / (1.0 - elements.eccentricity);
            elements.period = 2.0 * kPi * std::sqrt(elements.semiMajorAxis *
                                                    elements.semiMajorAxis *
                                                    elements.semiMajorAxis / mu);
        }
        // Orbit plane tilt measured from the XZ plane, whose normal is +Y --
        // the simulation lays the solar system out in XZ so that the spacetime
        // grid can sit in the same plane.
        elements.inclination = std::acos(std::clamp(angularMomentum.y / h, -1.0, 1.0));
    }

    elements.valid = true;
    return elements;
}

Vec3 circularOrbitVelocity(const Vec3& primaryPosition, double primaryMass,
                           const Vec3& position, const Vec3& orbitNormal, double G) {
    const Vec3 radius = position - primaryPosition;
    const double distance = glm::length(radius);
    if (distance <= 0.0 || primaryMass <= 0.0) return Vec3(0.0);

    Vec3 direction = glm::cross(orbitNormal, radius);
    if (lengthSquared(direction) <= 0.0) {
        // Radius parallel to the requested normal: pick any perpendicular.
        const Vec3 fallback = std::abs(radius.x) < std::abs(radius.z)
                                  ? Vec3(1.0, 0.0, 0.0)
                                  : Vec3(0.0, 0.0, 1.0);
        direction = glm::cross(fallback, radius);
    }
    return safeNormalize(direction) * circularOrbitSpeed(primaryMass, distance, G);
}

}  // namespace sim
