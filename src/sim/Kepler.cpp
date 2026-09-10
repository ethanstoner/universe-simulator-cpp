#include "sim/Kepler.h"

#include <algorithm>
#include <cmath>

namespace sim {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

double wrapToPi(double angle) {
    angle = std::fmod(angle + kPi, kTwoPi);
    if (angle < 0.0) angle += kTwoPi;
    return angle - kPi;
}

double wrapToTwoPi(double angle) {
    angle = std::fmod(angle, kTwoPi);
    if (angle < 0.0) angle += kTwoPi;
    return angle;
}

// The reference frame here is the XZ plane with +Y as its normal, but the
// textbook element formulae are written for an XY plane with +Z as its normal.
// Rather than rewrite them and get a sign wrong, the two vectors that cross the
// boundary are mapped. Both maps are proper rotations, so handedness -- and
// therefore the direction an orbit turns -- is preserved.
Vec3 eclipticToSim(const Vec3& v) { return Vec3(v.x, v.z, -v.y); }
Vec3 simToEcliptic(const Vec3& v) { return Vec3(v.x, -v.z, v.y); }

}  // namespace

double solveEccentricAnomaly(double meanAnomaly, double eccentricity) {
    const double e = std::clamp(eccentricity, 0.0, 0.999999);
    const double M = wrapToPi(meanAnomaly);
    if (e == 0.0) return M;

    // Danby's starter. The 0.85 is chosen so the guess stays on the correct
    // side of the root for every eccentricity below 1.
    double E = M + (M < 0.0 ? -0.85 * e : 0.85 * e);

    for (int iteration = 0; iteration < 200; ++iteration) {
        const double f = E - e * std::sin(E) - M;
        const double derivative = 1.0 - e * std::cos(E);
        if (derivative == 0.0) break;
        double step = -f / derivative;
        // See the header: unclamped, a near-parabolic orbit throws the iterate
        // into a different revolution and it never comes back.
        step = std::clamp(step, -1.0, 1.0);
        E += step;
        if (std::abs(f) < 1.0e-15) break;
    }
    return E;
}

double trueAnomalyFromEccentric(double eccentricAnomaly, double eccentricity) {
    const double e = std::clamp(eccentricity, 0.0, 0.999999);
    // The half-angle form, which is well conditioned everywhere on the ellipse.
    // Going through acos(cos nu) instead loses the sign near apoapsis and all
    // precision near periapsis.
    return 2.0 * std::atan2(std::sqrt(1.0 + e) * std::sin(0.5 * eccentricAnomaly),
                            std::sqrt(1.0 - e) * std::cos(0.5 * eccentricAnomaly));
}

double eccentricAnomalyFromTrue(double trueAnomaly, double eccentricity) {
    const double e = std::clamp(eccentricity, 0.0, 0.999999);
    return 2.0 * std::atan2(std::sqrt(1.0 - e) * std::sin(0.5 * trueAnomaly),
                            std::sqrt(1.0 + e) * std::cos(0.5 * trueAnomaly));
}

StateVector stateFromElements(const KeplerElements& elements, double mu) {
    StateVector state;
    const double a = elements.semiMajorAxis;
    const double e = std::clamp(elements.eccentricity, 0.0, 0.999999);
    if (a <= 0.0 || mu <= 0.0) return state;

    const double E = solveEccentricAnomaly(elements.meanAnomaly, e);
    const double cosE = std::cos(E);
    const double sinE = std::sin(E);
    const double factor = std::sqrt(1.0 - e * e);

    // Perifocal coordinates: the periapsis direction is the local x axis.
    const double x = a * (cosE - e);
    const double y = a * factor * sinE;
    const double r = a * (1.0 - e * cosE);
    if (r <= 0.0) return state;

    const double speedFactor = std::sqrt(mu * a) / r;
    const double vx = -speedFactor * sinE;
    const double vy = speedFactor * factor * cosE;

    const double cosO = std::cos(elements.longitudeOfAscendingNode);
    const double sinO = std::sin(elements.longitudeOfAscendingNode);
    const double cosw = std::cos(elements.argumentOfPeriapsis);
    const double sinw = std::sin(elements.argumentOfPeriapsis);
    const double cosi = std::cos(elements.inclination);
    const double sini = std::sin(elements.inclination);

    // Unit vector towards periapsis, and the one 90 degrees ahead of it in the
    // orbital plane. Rz(Omega) Rx(i) Rz(omega) applied to the perifocal axes.
    const Vec3 periapsisAxis(cosO * cosw - sinO * sinw * cosi,
                             sinO * cosw + cosO * sinw * cosi, sinw * sini);
    const Vec3 semiLatusAxis(-cosO * sinw - sinO * cosw * cosi,
                             -sinO * sinw + cosO * cosw * cosi, cosw * sini);

    state.position = eclipticToSim(periapsisAxis * x + semiLatusAxis * y);
    state.velocity = eclipticToSim(periapsisAxis * vx + semiLatusAxis * vy);
    return state;
}

KeplerElements elementsFromState(const Vec3& relativePosition,
                                 const Vec3& relativeVelocity, double mu) {
    KeplerElements elements;
    const Vec3 position = simToEcliptic(relativePosition);
    const Vec3 velocity = simToEcliptic(relativeVelocity);

    const double r = glm::length(position);
    if (r <= 0.0 || mu <= 0.0) return elements;

    const Vec3 angularMomentum = glm::cross(position, velocity);
    const double h = glm::length(angularMomentum);

    const double vSquared = lengthSquared(velocity);
    const double specificEnergy = 0.5 * vSquared - mu / r;
    if (specificEnergy >= 0.0) return elements;  // unbound: no ellipse to report
    elements.semiMajorAxis = -mu / (2.0 * specificEnergy);

    const Vec3 eccentricityVector =
        ((vSquared - mu / r) * position - glm::dot(position, velocity) * velocity) / mu;
    elements.eccentricity = glm::length(eccentricityVector);

    if (h <= 0.0) return elements;  // radial fall: the orbit plane is undefined
    elements.inclination = std::acos(std::clamp(angularMomentum.z / h, -1.0, 1.0));

    // Node vector: +Z cross h, which points along the ascending node.
    const Vec3 node(-angularMomentum.y, angularMomentum.x, 0.0);
    const double nodeLength = glm::length(node);

    const bool equatorial = nodeLength <= 1.0e-12 * h;
    const bool circular = elements.eccentricity <= 1.0e-12;

    // Argument of latitude: the angle from the reference direction to the body,
    // measured in the orbital plane. Every degenerate case below is expressed
    // through it, which is what keeps the round trip exact when there is no
    // node or no periapsis to measure from.
    double referenceToBody = 0.0;
    Vec3 reference(1.0, 0.0, 0.0);
    if (!equatorial) {
        reference = node / nodeLength;
        elements.longitudeOfAscendingNode = wrapToTwoPi(std::atan2(node.y, node.x));
    }
    {
        const Vec3 inPlane = glm::cross(angularMomentum / h, reference);
        referenceToBody = std::atan2(glm::dot(position, inPlane) / r,
                                     glm::dot(position, reference) / r);
    }

    double trueAnomaly = 0.0;
    if (circular) {
        // No periapsis exists, so put it at the reference direction and let the
        // mean anomaly carry the body's position.
        elements.argumentOfPeriapsis = 0.0;
        trueAnomaly = referenceToBody;
    } else {
        const Vec3 periapsis = eccentricityVector / elements.eccentricity;
        const Vec3 inPlane = glm::cross(angularMomentum / h, reference);
        elements.argumentOfPeriapsis = wrapToTwoPi(
            std::atan2(glm::dot(periapsis, inPlane), glm::dot(periapsis, reference)));
        trueAnomaly = referenceToBody - elements.argumentOfPeriapsis;
    }

    const double E = eccentricAnomalyFromTrue(trueAnomaly, elements.eccentricity);
    elements.meanAnomaly = wrapToTwoPi(E - elements.eccentricity * std::sin(E));
    return elements;
}

double meanMotion(const KeplerElements& elements, double mu) {
    const double a = elements.semiMajorAxis;
    if (a <= 0.0 || mu <= 0.0) return 0.0;
    return std::sqrt(mu / (a * a * a));
}

KeplerElements propagate(const KeplerElements& elements, double mu, double seconds) {
    KeplerElements advanced = elements;
    advanced.meanAnomaly =
        wrapToTwoPi(elements.meanAnomaly + meanMotion(elements, mu) * seconds);
    return advanced;
}

}  // namespace sim
