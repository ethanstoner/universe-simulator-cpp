#include "TestFramework.h"

#include <cmath>

#include "sim/Constants.h"
#include "sim/Diagnostics.h"
#include "sim/GravitySystem.h"
#include "sim/OrbitMath.h"

using namespace sim;

namespace {

CelestialBody makeBody(const char* name, double mass, Vec3 position, Vec3 velocity = Vec3(0.0)) {
    CelestialBody body;
    body.name = name;
    body.mass = mass;
    body.position = position;
    body.velocity = velocity;
    body.radius = 1.0;
    body.showTrail = false;
    return body;
}

GravitySystem makeBareSystem() {
    GravitySystem system;
    system.settings().stabilization = Stabilization::None;
    system.settings().trailLength = 0;
    return system;
}

}  // namespace

// --------------------------------------------------------------- vector maths

TEST(vec_safe_normalize_handles_zero) {
    CHECK(safeNormalize(Vec3(0.0)) == Vec3(0.0));
    const Vec3 unit = safeNormalize(Vec3(3.0, 4.0, 0.0));
    CHECK_NEAR(glm::length(unit), 1.0, 1e-15);
    CHECK_NEAR(unit.x, 0.6, 1e-15);
    CHECK_NEAR(unit.y, 0.8, 1e-15);
}

TEST(vec_length_squared_matches_dot) {
    const Vec3 v(1.5, -2.5, 3.0);
    CHECK_NEAR(lengthSquared(v), 1.5 * 1.5 + 2.5 * 2.5 + 9.0, 1e-15);
}

// ------------------------------------------------- gravitational acceleration

TEST(gravity_acceleration_matches_closed_form) {
    // Two 1 kg masses 1 m apart: each feels a = G * 1 / 1^2 = G.
    GravitySystem system = makeBareSystem();
    system.add(makeBody("a", 1.0, Vec3(0.0)));
    system.add(makeBody("b", 1.0, Vec3(1.0, 0.0, 0.0)));

    std::vector<Vec3> positions{Vec3(0.0), Vec3(1.0, 0.0, 0.0)};
    std::vector<Vec3> out;
    system.accelerations(positions, out);

    CHECK_REL(out[0].x, constants::kG, 1e-12);
    CHECK_NEAR(out[0].y, 0.0, 1e-30);
    CHECK_REL(out[1].x, -constants::kG, 1e-12);
}

TEST(gravity_falls_off_as_inverse_square) {
    GravitySystem system = makeBareSystem();
    const double M = 1.0e20;
    system.add(makeBody("central", M, Vec3(0.0)));
    system.add(makeBody("probe", 1.0, Vec3(100.0, 0.0, 0.0)));

    std::vector<Vec3> near{Vec3(0.0), Vec3(100.0, 0.0, 0.0)};
    std::vector<Vec3> far{Vec3(0.0), Vec3(200.0, 0.0, 0.0)};
    std::vector<Vec3> aNear, aFar;
    system.accelerations(near, aNear);
    system.accelerations(far, aFar);

    // Doubling the distance must quarter the acceleration.
    CHECK_REL(glm::length(aNear[1]) / glm::length(aFar[1]), 4.0, 1e-12);
    CHECK_REL(glm::length(aNear[1]), constants::kG * M / (100.0 * 100.0), 1e-12);
}

TEST(gravity_obeys_newtons_third_law) {
    // Unequal masses: accelerations differ, but m*a must be equal and opposite.
    GravitySystem system = makeBareSystem();
    const double m1 = 3.0e10, m2 = 7.0e12;
    system.add(makeBody("a", m1, Vec3(-4.0, 1.0, 2.0)));
    system.add(makeBody("b", m2, Vec3(5.0, -3.0, 0.5)));

    std::vector<Vec3> positions{system.bodies()[0].position, system.bodies()[1].position};
    std::vector<Vec3> out;
    system.accelerations(positions, out);

    const Vec3 f1 = out[0] * m1;
    const Vec3 f2 = out[1] * m2;
    const Vec3 sum = f1 + f2;
    CHECK_NEAR(glm::length(sum) / glm::length(f1), 0.0, 1e-14);
}

TEST(gravity_superposes_linearly) {
    // Two identical masses placed symmetrically about a probe must cancel.
    GravitySystem system = makeBareSystem();
    system.add(makeBody("probe", 0.0, Vec3(0.0)));
    system.add(makeBody("left", 1.0e15, Vec3(-10.0, 0.0, 0.0)));
    system.add(makeBody("right", 1.0e15, Vec3(10.0, 0.0, 0.0)));

    std::vector<Vec3> positions;
    for (const CelestialBody& body : system.bodies()) positions.push_back(body.position);
    std::vector<Vec3> out;
    system.accelerations(positions, out);

    const double reference = constants::kG * 1.0e15 / 100.0;
    CHECK_NEAR(glm::length(out[0]) / reference, 0.0, 1e-14);
}

TEST(gravity_fixed_body_never_accelerates) {
    GravitySystem system = makeBareSystem();
    CelestialBody anchor = makeBody("anchor", 1.0e30, Vec3(0.0));
    anchor.fixed = true;
    system.add(anchor);
    system.add(makeBody("satellite", 1.0, Vec3(1.0e7, 0.0, 0.0)));

    std::vector<Vec3> positions{Vec3(0.0), Vec3(1.0e7, 0.0, 0.0)};
    std::vector<Vec3> out;
    system.accelerations(positions, out);

    CHECK(out[0] == Vec3(0.0));
    CHECK(glm::length(out[1]) > 0.0);
}

// -------------------------------------------------------------- stabilisation

TEST(gravity_softening_bounds_acceleration_at_zero_separation) {
    GravitySystem system;
    system.settings().stabilization = Stabilization::Softening;
    system.settings().softeningLength = 1.0e3;
    system.add(makeBody("a", 1.0e24, Vec3(0.0)));
    system.add(makeBody("b", 1.0, Vec3(0.0)));  // exactly coincident

    std::vector<Vec3> positions{Vec3(0.0), Vec3(0.0)};
    std::vector<Vec3> out;
    system.accelerations(positions, out);

    // Coincident with softening: delta is zero so the acceleration is zero,
    // and crucially it is finite rather than NaN or inf.
    CHECK(std::isfinite(out[0].x) && std::isfinite(out[1].x));

    // Just off centre, softening must cap the magnitude well below the
    // unsoftened value.
    std::vector<Vec3> close{Vec3(0.0), Vec3(1.0, 0.0, 0.0)};
    system.accelerations(close, out);
    const double softened = glm::length(out[1]);
    const double unsoftened = constants::kG * 1.0e24 / 1.0;
    CHECK(std::isfinite(softened));
    CHECK(softened < unsoftened * 1e-8);
    // Analytic Plummer value: G M r / (r^2 + eps^2)^(3/2).
    const double epsSq = 1.0e3 * 1.0e3;
    const double expected = constants::kG * 1.0e24 * 1.0 / std::pow(1.0 + epsSq, 1.5);
    CHECK_REL(softened, expected, 1e-10);
}

TEST(gravity_min_distance_clamps_denominator) {
    GravitySystem system;
    system.settings().stabilization = Stabilization::MinDistance;
    system.settings().minimumDistance = 500.0;
    system.add(makeBody("a", 1.0e20, Vec3(0.0)));
    system.add(makeBody("b", 1.0, Vec3(1.0, 0.0, 0.0)));

    std::vector<Vec3> positions{Vec3(0.0), Vec3(1.0, 0.0, 0.0)};
    std::vector<Vec3> out;
    system.accelerations(positions, out);

    // Direction from the true separation, magnitude from the clamped one.
    const double expected = constants::kG * 1.0e20 * 1.0 / std::pow(500.0 * 500.0, 1.5);
    CHECK_REL(glm::length(out[1]), expected, 1e-12);
}

// ------------------------------------------------------------ orbital algebra

TEST(orbit_circular_speed_matches_earth) {
    // Earth's mean orbital speed is ~29.78 km/s.
    const double v = circularOrbitSpeed(constants::kSolarMass, constants::kAu);
    CHECK_NEAR(v, 29784.0, 60.0);
}

TEST(orbit_circular_speed_definition) {
    const double M = 1.234e28, r = 5.678e9;
    CHECK_REL(circularOrbitSpeed(M, r), std::sqrt(constants::kG * M / r), 1e-15);
}

TEST(orbit_escape_speed_is_root_two_times_circular) {
    const double M = constants::kSolarMass, r = constants::kAu;
    CHECK_REL(escapeSpeed(M, r) / circularOrbitSpeed(M, r), std::sqrt(2.0), 1e-14);
}

TEST(orbit_period_matches_one_year_at_one_au) {
    const double period = orbitalPeriod(constants::kSolarMass, constants::kAu);
    CHECK_NEAR(period / constants::kDay, 365.25, 1.0);
}

TEST(orbit_vis_viva_reduces_to_circular_when_r_equals_a) {
    const double M = constants::kSolarMass, r = constants::kAu;
    CHECK_REL(visVivaSpeed(M, r, r), circularOrbitSpeed(M, r), 1e-14);
}

TEST(orbit_elements_of_a_circular_orbit) {
    const double M = constants::kSolarMass;
    const double r = constants::kAu;
    const double mu = constants::kG * M;
    const Vec3 position(r, 0.0, 0.0);
    const Vec3 velocity(0.0, 0.0, circularOrbitSpeed(M, r));

    const OrbitalElements elements = computeOrbitalElements(position, velocity, mu);
    CHECK(elements.valid);
    CHECK(elements.bound);
    CHECK_NEAR(elements.eccentricity, 0.0, 1e-12);
    CHECK_REL(elements.semiMajorAxis, r, 1e-12);
    CHECK_REL(elements.periapsis, r, 1e-10);
    CHECK_REL(elements.apoapsis, r, 1e-10);
    CHECK_REL(elements.period, orbitalPeriod(M, r), 1e-12);
}

TEST(orbit_elements_detect_escape) {
    const double M = constants::kSolarMass;
    const double r = constants::kAu;
    const double mu = constants::kG * M;
    const Vec3 position(r, 0.0, 0.0);
    const Vec3 velocity(0.0, 0.0, escapeSpeed(M, r) * 1.1);

    const OrbitalElements elements = computeOrbitalElements(position, velocity, mu);
    CHECK(!elements.bound);
    CHECK(elements.eccentricity > 1.0);
}

TEST(orbit_circular_velocity_is_perpendicular_to_radius) {
    const Vec3 primary(0.0);
    const Vec3 position(constants::kAu, 0.0, 0.0);
    const Vec3 velocity = circularOrbitVelocity(primary, constants::kSolarMass, position,
                                                Vec3(0.0, 1.0, 0.0));
    CHECK_NEAR(glm::dot(safeNormalize(velocity), safeNormalize(position)), 0.0, 1e-14);
    CHECK_REL(glm::length(velocity), circularOrbitSpeed(constants::kSolarMass, constants::kAu),
              1e-14);
}

TEST(schwarzschild_radius_of_the_sun_is_about_three_km) {
    CHECK_NEAR(schwarzschildRadius(constants::kSolarMass) / 1000.0, 2.95, 0.02);
}

TEST(schwarzschild_radius_of_the_earth_is_about_nine_mm) {
    CHECK_NEAR(schwarzschildRadius(constants::kEarthMass) * 1000.0, 8.87, 0.05);
}

// ------------------------------------------------------------ centre of mass

TEST(two_body_centre_of_mass_is_mass_weighted) {
    GravitySystem system = makeBareSystem();
    system.add(makeBody("heavy", 3.0, Vec3(0.0)));
    system.add(makeBody("light", 1.0, Vec3(4.0, 0.0, 0.0)));

    const SystemDiagnostics diagnostics = computeDiagnostics(system);
    CHECK_NEAR(diagnostics.centreOfMass.x, 1.0, 1e-15);
    CHECK_NEAR(diagnostics.totalMass, 4.0, 1e-15);
}

TEST(diagnostics_kinetic_and_potential_energy) {
    GravitySystem system = makeBareSystem();
    system.add(makeBody("a", 2.0, Vec3(0.0), Vec3(3.0, 0.0, 0.0)));
    system.add(makeBody("b", 5.0, Vec3(10.0, 0.0, 0.0), Vec3(0.0, 0.0, 0.0)));

    const SystemDiagnostics d = computeDiagnostics(system);
    CHECK_NEAR(d.kineticEnergy, 0.5 * 2.0 * 9.0, 1e-15);
    CHECK_REL(d.potentialEnergy, -constants::kG * 2.0 * 5.0 / 10.0, 1e-14);
    CHECK_NEAR(d.totalEnergy, d.kineticEnergy + d.potentialEnergy, 1e-15);
}
