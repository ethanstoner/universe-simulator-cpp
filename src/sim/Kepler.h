#pragma once

#include "sim/Vec.h"

namespace sim {

// Classical Keplerian orbital elements and the conversion to and from a state
// vector.
//
// This is what lets a scene be specified the way an ephemeris publishes one --
// six numbers per body, at a stated epoch -- instead of as a circle at the
// semi-major axis. Everything here is closed-form two-body: it produces initial
// conditions, and the integrator takes over from there. Nothing in the running
// simulation consults these elements.
//
// Frame: the simulator lays its orbits out in the XZ plane with +Y as the
// plane's normal, so XZ stands in for the ecliptic and +Y for ecliptic north.
// A prograde orbit has its angular momentum along +Y; see OrbitMath's
// progradeTangent for the same convention applied to circular orbits.
struct KeplerElements {
    double semiMajorAxis = 0.0;             // a, metres
    double eccentricity = 0.0;              // e, in [0, 1)
    double inclination = 0.0;               // i, radians from the XZ plane
    double longitudeOfAscendingNode = 0.0;  // Omega, radians
    double argumentOfPeriapsis = 0.0;       // omega, radians from the node
    double meanAnomaly = 0.0;               // M at the epoch, radians
};

struct StateVector {
    Vec3 position{0.0};
    Vec3 velocity{0.0};
};

// Solves Kepler's equation M = E - e sin(E) for the eccentric anomaly.
//
// There is no closed form, so this is Newton-Raphson from Danby's starting
// guess with the step length clamped. The clamp is what keeps it convergent at
// high eccentricity: near periapsis of an e = 0.99 orbit the derivative
// 1 - e cos(E) falls to 0.01, and an unclamped Newton step overshoots by
// hundreds of radians and lands in a different revolution.
//
// The mean anomaly is wrapped first, so the result is in [-pi, pi] regardless
// of how many revolutions the caller has accumulated.
double solveEccentricAnomaly(double meanAnomaly, double eccentricity);

double trueAnomalyFromEccentric(double eccentricAnomaly, double eccentricity);
double eccentricAnomalyFromTrue(double trueAnomaly, double eccentricity);

// Position and velocity RELATIVE TO THE PRIMARY, with mu = G(M + m).
StateVector stateFromElements(const KeplerElements& elements, double mu);

// The inverse. Degenerate orbits are resolved rather than left undefined: a
// circular orbit has no periapsis to measure the argument from, so omega is
// reported as 0 and the position angle folded into the mean anomaly; an
// equatorial orbit has no ascending node, so Omega is reported as 0 and folded
// into omega. Round-tripping through stateFromElements returns the same state
// either way.
KeplerElements elementsFromState(const Vec3& relativePosition,
                                 const Vec3& relativeVelocity, double mu);

// Mean motion n = sqrt(mu / a^3), radians per second.
double meanMotion(const KeplerElements& elements, double mu);

// Advances the mean anomaly by `seconds`. This is the analytic two-body
// solution -- exact, and completely blind to every other body in the scene,
// which is precisely why the simulator integrates instead.
KeplerElements propagate(const KeplerElements& elements, double mu, double seconds);

}  // namespace sim
