#pragma once

#include "sim/Vec.h"

namespace sim {

// The simulator lays its orbits out in the XZ plane, so +Y is the plane's
// normal and stands in for ecliptic north. A PROGRADE orbit -- the sense every
// planet in the real solar system travels in when viewed from the north -- has
// its angular momentum along +Y.
//
// `progradeTangent` is the unit tangent at phase angle t = atan2(z, x) for such
// an orbit. Everything that assigns an orbital velocity goes through it, so
// that no two bodies in a scene end up counter-orbiting and so that
// `computeOrbitalElements` reports a coplanar orbit as inclined 0 rather
// than 180 degrees.
//
// It points towards DECREASING phase angle: a body laid out at angle t and sent
// towards +t carries angular momentum along -Y, which is retrograde.
Vec3 progradeTangent(double phaseRadians);

// +1 if the orbit runs prograde (angular momentum along +Y), -1 if retrograde.
// Multiply a phase-angle rate by this to express it in the direction the body
// is actually travelling, which makes the measurement independent of which way
// round the scene was laid out.
double orbitSense(const Vec3& relativePosition, const Vec3& relativeVelocity);

// Speed for a circular orbit of radius r around mass M: v = sqrt(GM/r).
double circularOrbitSpeed(double centralMass, double radius,
                          double G = 6.67430e-11);

// Speed at radius r on an ellipse of semi-major axis a (vis-viva):
//   v^2 = GM (2/r - 1/a)
double visVivaSpeed(double centralMass, double radius, double semiMajorAxis,
                    double G = 6.67430e-11);

// Escape speed: v = sqrt(2GM/r).
double escapeSpeed(double centralMass, double radius, double G = 6.67430e-11);

// Period of a circular/elliptical orbit: T = 2 pi sqrt(a^3 / GM).
double orbitalPeriod(double centralMass, double semiMajorAxis,
                     double G = 6.67430e-11);

// Schwarzschild radius r_s = 2GM/c^2. Reported for reference only: this
// simulator is Newtonian and does not implement an event horizon.
double schwarzschildRadius(double mass, double G = 6.67430e-11,
                           double c = 299792458.0);

// Perihelion distance and speed for an ellipse of semi-major axis a and
// eccentricity e about a central mass. Starting a body at perihelion is the
// cleanest way to set up an eccentric orbit: the velocity is purely tangential
// there, so no vector decomposition is needed.
double perihelionDistance(double semiMajorAxis, double eccentricity);
double perihelionSpeed(double centralMass, double semiMajorAxis, double eccentricity,
                       double G = 6.67430e-11);

// Predicted relativistic perihelion advance per orbit, in radians:
//     6 pi G M / (c^2 a (1 - e^2))
// The closed-form value the simulation's 1PN correction should reproduce.
double relativisticPerihelionAdvance(double centralMass, double semiMajorAxis,
                                     double eccentricity, double G = 6.67430e-11,
                                     double c = 299792458.0);

// Direction of periapsis, as the argument of the eccentricity (Laplace-Runge-
// Lenz) vector projected into the XZ plane. Constant for a closed Kepler
// ellipse; it rotates when the orbit precesses, which is what makes it the
// thing to measure.
double periapsisAngle(const Vec3& relativePosition, const Vec3& relativeVelocity,
                      double mu);

// Osculating two-body orbital elements of a test body relative to a primary.
struct OrbitalElements {
    bool valid = false;
    double semiMajorAxis = 0.0;   // m; negative for hyperbolic orbits
    double eccentricity = 0.0;
    double periapsis = 0.0;       // m
    double apoapsis = 0.0;        // m; 0 when unbound
    double period = 0.0;          // s; 0 when unbound
    double specificEnergy = 0.0;  // J/kg
    double inclination = 0.0;     // radians from the XZ plane's normal (+Y)
    bool bound = false;
};

// `relativePosition` and `relativeVelocity` are the secondary's state minus the
// primary's. `mu = G(M + m)`, the standard gravitational parameter.
OrbitalElements computeOrbitalElements(const Vec3& relativePosition,
                                       const Vec3& relativeVelocity, double mu);

// Given a primary and a desired circular orbit, returns the velocity vector a
// body at `position` needs: perpendicular to the radius, in the plane defined
// by the radius and `orbitNormal`.
Vec3 circularOrbitVelocity(const Vec3& primaryPosition, double primaryMass,
                           const Vec3& position, const Vec3& orbitNormal,
                           double G = 6.67430e-11);

}  // namespace sim
