#include "TestFramework.h"

#include <cmath>
#include <cstdio>

#include "sim/Constants.h"
#include "sim/GravitySystem.h"
#include "sim/Kepler.h"
#include "sim/OrbitMath.h"
#include "sim/SceneLibrary.h"

using namespace sim;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kSunMu = constants::kG * constants::kSolarMass;

// Deterministic element sets spanning the awkward cases: circular, equatorial,
// both at once, steeply inclined, retrograde, and very eccentric.
struct NamedElements {
    const char* name;
    KeplerElements elements;
};

const NamedElements kCases[] = {
    {"circular equatorial", {constants::kAu, 0.0, 0.0, 0.0, 0.0, 0.0}},
    {"circular equatorial, part way round",
     {constants::kAu, 0.0, 0.0, 0.0, 0.0, 2.4}},
    {"eccentric equatorial", {constants::kAu, 0.4, 0.0, 0.0, 1.1, 0.9}},
    {"circular inclined", {2.0 * constants::kAu, 0.0, 0.5, 1.9, 0.0, 3.3}},
    {"general", {1.7 * constants::kAu, 0.31, 0.42, 2.7, 1.3, 0.6}},
    {"steeply inclined", {0.6 * constants::kAu, 0.62, 1.42, 5.1, 4.4, 5.9}},
    {"retrograde", {1.1 * constants::kAu, 0.24, 2.9, 0.7, 2.2, 1.7}},
    {"comet-like", {12.0 * constants::kAu, 0.94, 0.3, 3.4, 0.2, 0.05}},
};

double angleDifference(double a, double b) {
    double delta = std::fmod(a - b, 2.0 * kPi);
    if (delta > kPi) delta -= 2.0 * kPi;
    if (delta < -kPi) delta += 2.0 * kPi;
    return delta;
}

}  // namespace

TEST(kepler_equation_is_solved_across_every_eccentricity) {
    // The defining property: whatever E comes back must satisfy
    // M = E - e sin(E). Checked on a grid rather than at a few points, because
    // the solver's difficulty is concentrated near periapsis of a high
    // eccentricity orbit and a coarse sample walks straight past it.
    double worst = 0.0;
    for (int ei = 0; ei <= 20; ++ei) {
        const double e = 0.99 * ei / 20.0;
        for (int mi = 0; mi <= 200; ++mi) {
            const double M = -kPi + 2.0 * kPi * mi / 200.0;
            const double E = solveEccentricAnomaly(M, e);
            const double residual = std::abs(angleDifference(E - e * std::sin(E), M));
            worst = std::max(worst, residual);
        }
    }
    std::printf("         Kepler solver worst residual over e in [0, 0.99]: %.2e rad\n",
                worst);
    CHECK_LESS(worst, 1e-12);
}

TEST(kepler_equation_is_the_identity_for_a_circle) {
    for (int i = 0; i <= 12; ++i) {
        const double M = -kPi + 2.0 * kPi * i / 12.0;
        CHECK_NEAR(angleDifference(solveEccentricAnomaly(M, 0.0), M), 0.0, 1e-15);
    }
}

TEST(kepler_anomaly_conversions_invert_each_other) {
    for (double e : {0.0, 0.1, 0.5, 0.9, 0.99}) {
        for (int i = 0; i <= 24; ++i) {
            const double E = -kPi + 2.0 * kPi * i / 24.0;
            const double nu = trueAnomalyFromEccentric(E, e);
            CHECK_NEAR(angleDifference(eccentricAnomalyFromTrue(nu, e), E), 0.0, 1e-12);
        }
    }
}

TEST(kepler_elements_round_trip_through_a_state_vector) {
    // Six numbers in, six out. The degenerate cases are the point: a circular
    // orbit has no periapsis and an equatorial one has no ascending node, so
    // omega and Omega are not individually recoverable there. What must survive
    // is the STATE, which is what the simulator actually consumes.
    for (const NamedElements& testCase : kCases) {
        const StateVector state = stateFromElements(testCase.elements, kSunMu);
        const KeplerElements recovered =
            elementsFromState(state.position, state.velocity, kSunMu);
        const StateVector again = stateFromElements(recovered, kSunMu);

        const double scale = glm::length(state.position);
        const double speed = glm::length(state.velocity);
        CHECK(scale > 0.0);
        CHECK_LESS(glm::length(again.position - state.position) / scale, 1e-11);
        CHECK_LESS(glm::length(again.velocity - state.velocity) / speed, 1e-11);

        // a, e and i are unambiguous in every case, degenerate or not.
        CHECK_REL(recovered.semiMajorAxis, testCase.elements.semiMajorAxis, 1e-12);
        CHECK_NEAR(recovered.eccentricity, testCase.elements.eccentricity, 1e-12);
        CHECK_NEAR(recovered.inclination, testCase.elements.inclination, 1e-12);
    }
}

TEST(kepler_non_degenerate_elements_come_back_individually) {
    // Where the orbit is neither circular nor equatorial, all six elements are
    // separately determined and must each survive the round trip.
    for (const NamedElements& testCase : kCases) {
        if (testCase.elements.eccentricity < 1e-6) continue;
        if (testCase.elements.inclination < 1e-6) continue;

        const StateVector state = stateFromElements(testCase.elements, kSunMu);
        const KeplerElements recovered =
            elementsFromState(state.position, state.velocity, kSunMu);

        CHECK_NEAR(angleDifference(recovered.longitudeOfAscendingNode,
                                   testCase.elements.longitudeOfAscendingNode),
                   0.0, 1e-10);
        CHECK_NEAR(angleDifference(recovered.argumentOfPeriapsis,
                                   testCase.elements.argumentOfPeriapsis),
                   0.0, 1e-10);
        CHECK_NEAR(angleDifference(recovered.meanAnomaly, testCase.elements.meanAnomaly),
                   0.0, 1e-10);
    }
}

TEST(kepler_state_satisfies_the_conserved_quantities) {
    // Independent of the round trip: the state a set of elements produces must
    // have the specific energy and angular momentum the elements imply.
    for (const NamedElements& testCase : kCases) {
        const StateVector state = stateFromElements(testCase.elements, kSunMu);
        const double a = testCase.elements.semiMajorAxis;
        const double e = testCase.elements.eccentricity;

        const double energy = 0.5 * lengthSquared(state.velocity) -
                              kSunMu / glm::length(state.position);
        CHECK_REL(energy, -kSunMu / (2.0 * a), 1e-12);

        const Vec3 h = glm::cross(state.position, state.velocity);
        CHECK_REL(glm::length(h), std::sqrt(kSunMu * a * (1.0 - e * e)), 1e-12);

        // Inclination is the tilt of the orbit plane from XZ, whose normal is
        // +Y -- so cos(i) is exactly the normalised y component of h.
        CHECK_NEAR(h.y / glm::length(h), std::cos(testCase.elements.inclination), 1e-12);
    }
}

TEST(kepler_zero_mean_anomaly_starts_at_periapsis) {
    for (const NamedElements& testCase : kCases) {
        KeplerElements elements = testCase.elements;
        elements.meanAnomaly = 0.0;
        const StateVector state = stateFromElements(elements, kSunMu);

        const double r = glm::length(state.position);
        CHECK_REL(r, elements.semiMajorAxis * (1.0 - elements.eccentricity), 1e-12);
        // At an apsis the velocity is perpendicular to the radius.
        CHECK_NEAR(glm::dot(safeNormalize(state.position), safeNormalize(state.velocity)),
                   0.0, 1e-12);
    }
}

TEST(kepler_half_a_period_from_periapsis_reaches_apoapsis) {
    KeplerElements elements{constants::kAu, 0.4, 0.0, 0.0, 0.0, 0.0};
    const double period = 2.0 * kPi / meanMotion(elements, kSunMu);
    const KeplerElements later = propagate(elements, kSunMu, 0.5 * period);

    const StateVector state = stateFromElements(later, kSunMu);
    CHECK_REL(glm::length(state.position),
              elements.semiMajorAxis * (1.0 + elements.eccentricity), 1e-10);
    CHECK_REL(glm::length(state.velocity),
              visVivaSpeed(constants::kSolarMass,
                           elements.semiMajorAxis * (1.0 + elements.eccentricity),
                           elements.semiMajorAxis),
              1e-10);
}

TEST(kepler_a_full_period_returns_to_the_same_state) {
    for (const NamedElements& testCase : kCases) {
        const double period = 2.0 * kPi / meanMotion(testCase.elements, kSunMu);
        const StateVector start = stateFromElements(testCase.elements, kSunMu);
        const StateVector end =
            stateFromElements(propagate(testCase.elements, kSunMu, period), kSunMu);
        CHECK_LESS(glm::length(end.position - start.position) / glm::length(start.position),
                   1e-10);
    }
}

TEST(kepler_period_matches_the_closed_form) {
    KeplerElements elements{constants::kAu, 0.0167, 0.0, 0.0, 0.0, 0.0};
    const double period = 2.0 * kPi / meanMotion(elements, kSunMu);
    CHECK_REL(period, orbitalPeriod(constants::kSolarMass, constants::kAu), 1e-12);
    CHECK_NEAR(period / constants::kJulianYear, 1.0, 0.001);
}

TEST(kepler_agrees_with_the_osculating_element_code) {
    // Two independent implementations of the same quantities: Kepler.cpp works
    // in the perifocal frame, OrbitMath.cpp from the eccentricity vector. They
    // must agree, and disagreeing would mean the inspector and the scene
    // loader disagree about what orbit a body is on.
    for (const NamedElements& testCase : kCases) {
        const StateVector state = stateFromElements(testCase.elements, kSunMu);
        const OrbitalElements osculating =
            computeOrbitalElements(state.position, state.velocity, kSunMu);

        CHECK(osculating.valid);
        CHECK(osculating.bound);
        CHECK_REL(osculating.semiMajorAxis, testCase.elements.semiMajorAxis, 1e-10);
        CHECK_NEAR(osculating.eccentricity, testCase.elements.eccentricity, 1e-10);
        CHECK_NEAR(osculating.inclination, testCase.elements.inclination, 1e-10);
        CHECK_REL(osculating.periapsis,
                  testCase.elements.semiMajorAxis * (1.0 - testCase.elements.eccentricity),
                  1e-10);
    }
}

TEST(kepler_matches_the_circular_orbit_helper_the_presets_use) {
    // The bridge to the existing scenes. A body placed by placeInCircularOrbit
    // at phase t is the same body as one with zero eccentricity and zero
    // inclination -- as long as both agree on which way an orbit turns.
    //
    // The phase angle atan2(z, x) runs opposite to the orbit's direction of
    // travel, so the periapsis longitude that corresponds to phase t is -t.
    CelestialBody primary = makeBody("primary", constants::kSolarMass, 6.96e8, Vec3(0.0),
                                     Vec3(0.0), glm::vec3(1.0f));
    CelestialBody body = makeBody("body", 0.0, 1.0, Vec3(0.0), Vec3(0.0), glm::vec3(1.0f));

    for (double phase : {0.0, 0.7, 2.9, -1.4, 5.5}) {
        placeInCircularOrbit(body, primary, constants::kAu, constants::kG, phase);

        KeplerElements elements;
        elements.semiMajorAxis = constants::kAu;
        elements.meanAnomaly = -phase;
        const StateVector state = stateFromElements(elements, kSunMu);

        CHECK_LESS(glm::length(state.position - body.position) / constants::kAu, 1e-12);
        CHECK_LESS(glm::length(state.velocity - body.velocity) /
                       glm::length(body.velocity),
                   1e-12);
    }
}

TEST(kepler_matches_the_elliptical_orbit_helper_the_presets_use) {
    CelestialBody primary = makeBody("primary", constants::kSolarMass, 6.96e8, Vec3(0.0),
                                     Vec3(0.0), glm::vec3(1.0f));
    CelestialBody body = makeBody("body", 0.0, 1.0, Vec3(0.0), Vec3(0.0), glm::vec3(1.0f));

    const double a = 5.790905e10;
    const double e = 0.205630;
    for (double phase : {0.0, 1.2, -2.6}) {
        placeInEllipticalOrbit(body, primary, a, e, constants::kG, phase);

        // placeInEllipticalOrbit starts the body AT periapsis, so the mean
        // anomaly is zero and the periapsis direction is the phase angle.
        KeplerElements elements;
        elements.semiMajorAxis = a;
        elements.eccentricity = e;
        elements.argumentOfPeriapsis = -phase;
        const StateVector state = stateFromElements(elements, kSunMu);

        CHECK_LESS(glm::length(state.position - body.position) / a, 1e-12);
        CHECK_LESS(glm::length(state.velocity - body.velocity) /
                       glm::length(body.velocity),
                   1e-12);
    }
}

TEST(kepler_initial_conditions_survive_the_integrator) {
    // End to end: build a state from elements, hand it to the real integrator,
    // and require the orbit it produces to have the elements it started with.
    // A sign error in the frame mapping would show up here as an orbit that is
    // the right shape but the wrong way round.
    const KeplerElements elements{constants::kAu, 0.2, 0.35, 1.1, 2.3, 0.8};
    const StateVector state = stateFromElements(elements, kSunMu);

    GravitySystem system;
    system.settings().stabilization = Stabilization::None;
    system.settings().trailLength = 0;

    CelestialBody sun;
    sun.name = "Sun";
    sun.mass = constants::kSolarMass;
    sun.radius = 6.96e8;
    sun.fixed = true;
    system.add(sun);

    CelestialBody planet;
    planet.name = "Planet";
    planet.mass = 0.0;
    planet.radius = 1.0e6;
    planet.position = state.position;
    planet.velocity = state.velocity;
    system.add(planet);

    const double period = 2.0 * kPi / meanMotion(elements, kSunMu);
    const double dt = period / 20000.0;
    const long long steps = static_cast<long long>(period / dt);
    for (long long i = 0; i < steps; ++i) system.step(dt);

    const CelestialBody& integrated = system.bodies()[1];
    const KeplerElements after =
        elementsFromState(integrated.position, integrated.velocity, kSunMu);

    CHECK_REL(after.semiMajorAxis, elements.semiMajorAxis, 1e-6);
    CHECK_NEAR(after.eccentricity, elements.eccentricity, 1e-6);
    CHECK_NEAR(after.inclination, elements.inclination, 1e-9);
    CHECK_NEAR(angleDifference(after.longitudeOfAscendingNode,
                               elements.longitudeOfAscendingNode),
               0.0, 1e-9);
    // One full period later the body is back where it started.
    CHECK_NEAR(angleDifference(after.meanAnomaly, elements.meanAnomaly), 0.0, 1e-4);
}
