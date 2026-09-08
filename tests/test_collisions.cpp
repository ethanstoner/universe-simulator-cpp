#include "TestFramework.h"

#include <cmath>

#include "sim/Collisions.h"
#include "sim/Constants.h"
#include "sim/GravitySystem.h"

using namespace sim;

namespace {
constexpr double kPi = 3.14159265358979323846;

CelestialBody sphere(const char* name, double mass, double radius, Vec3 position,
                     Vec3 velocity) {
    CelestialBody body;
    body.name = name;
    body.mass = mass;
    body.radius = radius;
    body.position = position;
    body.velocity = velocity;
    body.showTrail = false;
    return body;
}
}  // namespace

TEST(merge_conserves_mass_and_momentum) {
    CelestialBody a = sphere("a", 3.0, 1.0, Vec3(0.0), Vec3(4.0, 0.0, 0.0));
    CelestialBody b = sphere("b", 1.0, 1.0, Vec3(1.0, 0.0, 0.0), Vec3(-8.0, 0.0, 0.0));
    const Vec3 momentumBefore = a.momentum() + b.momentum();

    mergeInto(a, b);

    CHECK_NEAR(a.mass, 4.0, 1e-15);
    CHECK_NEAR(glm::length(a.momentum() - momentumBefore), 0.0, 1e-13);
    CHECK_NEAR(a.velocity.x, (3.0 * 4.0 + 1.0 * -8.0) / 4.0, 1e-15);
}

TEST(merge_places_result_at_the_centre_of_mass) {
    CelestialBody a = sphere("a", 3.0, 1.0, Vec3(0.0), Vec3(0.0));
    CelestialBody b = sphere("b", 1.0, 1.0, Vec3(4.0, 0.0, 0.0), Vec3(0.0));
    mergeInto(a, b);
    CHECK_NEAR(a.position.x, 1.0, 1e-15);
}

TEST(merge_adds_volumes_not_radii) {
    CelestialBody a = sphere("a", 1.0, 2.0, Vec3(0.0), Vec3(0.0));
    CelestialBody b = sphere("b", 1.0, 2.0, Vec3(0.0), Vec3(0.0));
    mergeInto(a, b);
    // Two equal spheres: the combined radius is 2 * cbrt(2), not 4.
    CHECK_REL(a.radius, 2.0 * std::cbrt(2.0), 1e-14);
}

TEST(merge_preserves_density_of_identical_bodies) {
    CelestialBody a = sphere("a", 1000.0, 3.0, Vec3(0.0), Vec3(0.0));
    CelestialBody b = sphere("b", 1000.0, 3.0, Vec3(0.0), Vec3(0.0));
    const double densityBefore = a.density();
    mergeInto(a, b);
    CHECK_REL(a.density(), densityBefore, 1e-14);
}

TEST(collision_merge_removes_the_absorbed_body) {
    GravitySystem system;
    system.settings().collisionMode = CollisionMode::Merge;
    system.settings().pairwiseGravityEnabled = false;
    system.settings().trailLength = 0;

    system.add(sphere("big", 1.0e10, 100.0, Vec3(0.0), Vec3(0.0)));
    system.add(sphere("small", 1.0, 10.0, Vec3(50.0, 0.0, 0.0), Vec3(0.0)));

    const StepReport report = system.step(1.0);
    CHECK(report.merges == 1);
    CHECK(system.size() == 1);
    CHECK(system.bodies()[0].name == "big");  // the heavier body keeps its identity
    CHECK_REL(system.bodies()[0].mass, 1.0e10 + 1.0, 1e-15);
}

TEST(collision_merge_keeps_bodies_that_do_not_touch) {
    GravitySystem system;
    system.settings().collisionMode = CollisionMode::Merge;
    system.settings().pairwiseGravityEnabled = false;
    system.settings().trailLength = 0;

    system.add(sphere("a", 1.0, 1.0, Vec3(0.0), Vec3(0.0)));
    system.add(sphere("b", 1.0, 1.0, Vec3(100.0, 0.0, 0.0), Vec3(0.0)));

    const StepReport report = system.step(1.0);
    CHECK(report.merges == 0);
    CHECK(system.size() == 2);
}

TEST(collision_elastic_conserves_momentum_and_energy_when_restitution_is_one) {
    GravitySystem system;
    system.settings().collisionMode = CollisionMode::Elastic;
    system.settings().collisionRestitution = 1.0;
    system.settings().pairwiseGravityEnabled = false;
    system.settings().trailLength = 0;

    system.add(sphere("a", 2.0, 1.0, Vec3(-0.9, 0.0, 0.0), Vec3(3.0, 0.0, 0.0)));
    system.add(sphere("b", 4.0, 1.0, Vec3(0.9, 0.0, 0.0), Vec3(-1.0, 0.0, 0.0)));

    const Vec3 momentumBefore =
        system.bodies()[0].momentum() + system.bodies()[1].momentum();
    const double energyBefore =
        system.bodies()[0].kineticEnergy() + system.bodies()[1].kineticEnergy();

    system.step(1e-9);  // step is tiny so free flight contributes nothing

    const Vec3 momentumAfter =
        system.bodies()[0].momentum() + system.bodies()[1].momentum();
    const double energyAfter =
        system.bodies()[0].kineticEnergy() + system.bodies()[1].kineticEnergy();

    CHECK_NEAR(glm::length(momentumAfter - momentumBefore), 0.0, 1e-12);
    CHECK_REL(energyAfter, energyBefore, 1e-12);
}

TEST(collision_elastic_separates_overlapping_bodies) {
    GravitySystem system;
    system.settings().collisionMode = CollisionMode::Elastic;
    system.settings().pairwiseGravityEnabled = false;
    system.settings().trailLength = 0;

    system.add(sphere("a", 1.0, 1.0, Vec3(-0.5, 0.0, 0.0), Vec3(1.0, 0.0, 0.0)));
    system.add(sphere("b", 1.0, 1.0, Vec3(0.5, 0.0, 0.0), Vec3(-1.0, 0.0, 0.0)));

    system.step(1e-9);
    const double separation =
        glm::length(system.bodies()[1].position - system.bodies()[0].position);
    CHECK(separation >= 2.0 - 1e-9);
}

TEST(collision_ignore_mode_leaves_bodies_untouched) {
    GravitySystem system;
    system.settings().collisionMode = CollisionMode::Ignore;
    system.settings().pairwiseGravityEnabled = false;
    system.settings().trailLength = 0;

    system.add(sphere("a", 1.0, 1.0, Vec3(0.0), Vec3(0.0)));
    system.add(sphere("b", 1.0, 1.0, Vec3(0.1, 0.0, 0.0), Vec3(0.0)));

    system.step(1.0);
    CHECK(system.size() == 2);
}

TEST(body_density_and_radius_round_trip) {
    // Earth's mass at Earth's mean density must give back Earth's radius.
    CelestialBody body;
    body.mass = constants::kEarthMass;
    body.setRadiusFromDensity(5514.0);
    CHECK_NEAR(body.radius / 1000.0, 6371.0, 5.0);
    CHECK_REL(body.density(), 5514.0, 1e-12);
}

TEST(body_density_of_a_sphere_matches_the_formula) {
    CelestialBody body;
    body.mass = 12.0;
    body.radius = 2.0;
    const double volume = (4.0 / 3.0) * kPi * 8.0;
    CHECK_REL(body.density(), 12.0 / volume, 1e-14);
}
