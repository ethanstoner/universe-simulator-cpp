#include "TestFramework.h"

#include <cmath>

#include "sim/Constants.h"
#include "sim/GravitySystem.h"
#include "sim/SceneLibrary.h"
#include "sim/ViewMath.h"

using namespace sim;

namespace {

CelestialBody named(const char* name, double radius, Vec3 position) {
    CelestialBody body;
    body.name = name;
    body.radius = radius;
    body.mass = 1.0e24;
    body.position = position;
    body.showTrail = false;
    return body;
}

ViewScale scaleFor(double metresPerUnit, double gain, double exponent = 0.35) {
    ViewScale scale;
    scale.metresPerUnit = metresPerUnit;
    scale.gain = gain;
    scale.exponent = exponent;
    scale.minRadius = 0.0;
    return scale;
}

}  // namespace

// ---------------------------------------------------------- floating origin

TEST(view_relative_position_subtracts_in_double) {
    // A body one metre beyond 1 AU. If the subtraction happened after a
    // narrowing to float the metre would vanish entirely: float spacing at
    // 1.5e11 is about 16 384.
    CelestialBody body = named("probe", 1.0, Vec3(constants::kAu + 1.0, 0.0, 0.0));
    const Vec3 camera(constants::kAu, 0.0, 0.0);  // metresPerUnit = 1
    const Vec3 relative = relativePosition(body, camera, 1.0);
    CHECK_NEAR(relative.x, 1.0, 1e-9);
}

TEST(view_relative_position_scales_by_metres_per_unit) {
    CelestialBody body = named("probe", 1.0, Vec3(constants::kAu, 0.0, 0.0));
    const Vec3 relative = relativePosition(body, Vec3(0.0), constants::kAu / 10.0);
    CHECK_NEAR(relative.x, 10.0, 1e-12);
}

// ------------------------------------------------------------ visual radius

TEST(view_true_scale_returns_the_real_radius) {
    ViewScale scale = scaleFor(constants::kAu / 10.0, 999.0);
    scale.trueScale = true;
    CelestialBody earth = named("Earth", constants::kEarthRadius, Vec3(0.0));
    CHECK_REL(visualRadius(earth, scale),
              constants::kEarthRadius / (constants::kAu / 10.0), 1e-12);
}

TEST(view_power_law_preserves_size_ordering) {
    // Compression must never reorder bodies: a bigger body always draws bigger.
    const ViewScale scale = scaleFor(constants::kAu / 10.0, 3.0);
    const double sun = visualRadius(named("Sun", 6.9634e8, Vec3(0.0)), scale);
    const double jupiter = visualRadius(named("Jupiter", 6.9911e7, Vec3(0.0)), scale);
    const double earth = visualRadius(named("Earth", 6.371e6, Vec3(0.0)), scale);
    const double moon = visualRadius(named("Moon", 1.7374e6, Vec3(0.0)), scale);
    CHECK(sun > jupiter);
    CHECK(jupiter > earth);
    CHECK(earth > moon);
}

TEST(view_power_law_compresses_the_sun_to_earth_ratio) {
    // The physical ratio is 109:1. At exponent 0.35 it must come out far
    // smaller -- that is the entire reason the power law exists, because a
    // linear exaggeration draws the Sun wider than the Earth's orbit.
    const ViewScale scale = scaleFor(constants::kAu / 10.0, 3.0);
    const double sun = visualRadius(named("Sun", 6.9634e8, Vec3(0.0)), scale);
    const double earth = visualRadius(named("Earth", 6.371e6, Vec3(0.0)), scale);

    const double physicalRatio = 6.9634e8 / 6.371e6;
    CHECK_NEAR(physicalRatio, 109.3, 0.5);
    const double drawnRatio = sun / earth;
    CHECK(drawnRatio > 3.0);
    CHECK(drawnRatio < 8.0);
    // And the Sun must stay well inside the Earth's 10 unit orbit.
    CHECK_LESS(sun, 3.0);
}

TEST(view_radius_respects_the_clamps) {
    ViewScale scale = scaleFor(1.0, 1.0);
    scale.minRadius = 0.5;
    scale.maxRadius = 2.0;
    CHECK_NEAR(visualRadius(named("tiny", 1e-9, Vec3(0.0)), scale), 0.5, 1e-12);
    CHECK_NEAR(visualRadius(named("huge", 1e9, Vec3(0.0)), scale), 2.0, 1e-12);
}

TEST(view_zero_radius_body_gets_the_minimum) {
    ViewScale scale = scaleFor(1.0, 1.0);
    scale.minRadius = 0.25;
    CHECK_NEAR(visualRadius(named("point", 0.0, Vec3(0.0)), scale), 0.25, 1e-12);
}

// -------------------------------------------------------------- gain solving

TEST(view_gain_makes_the_reference_body_hit_its_target) {
    GravitySystem system;
    applyScene(*findScene("inner"), system);
    const double metresPerUnit = constants::kAu / 10.0;

    ViewScale scale = scaleFor(metresPerUnit, 1.0);
    scale.gain = solveVisualGain(system, metresPerUnit, scale.exponent, 2.4, "Sun");

    const CelestialBody* sun = nullptr;
    for (const CelestialBody& body : system.bodies()) {
        if (body.name == "Sun") sun = &body;
    }
    CHECK(sun != nullptr);
    CHECK_REL(visualRadius(*sun, scale), 2.4, 1e-12);
}

TEST(view_gain_falls_back_to_the_largest_body) {
    GravitySystem system;
    applyScene(*findScene("inner"), system);
    const double metresPerUnit = constants::kAu / 10.0;

    // An unknown reference name must not silently produce gain 1.
    const double named = solveVisualGain(system, metresPerUnit, 0.35, 2.4, "Nonexistent");
    const double largest = solveVisualGain(system, metresPerUnit, 0.35, 2.4);
    CHECK_REL(named, largest, 1e-15);
    CHECK(largest > 0.0);
}

TEST(view_gain_uses_the_named_body_not_the_largest) {
    // The regression this exists for: the Earth-Moon preset still contains the
    // Sun, so solving from "largest" made the Earth six pixels across.
    GravitySystem system;
    applyScene(*findScene("earth-moon"), system);
    const double metresPerUnit = 5.0e7;

    const double fromEarth = solveVisualGain(system, metresPerUnit, 0.35, 0.55, "Earth");
    const double fromLargest = solveVisualGain(system, metresPerUnit, 0.35, 0.55);
    CHECK(fromEarth > fromLargest * 3.0);

    ViewScale scale = scaleFor(metresPerUnit, fromEarth);
    for (const CelestialBody& body : system.bodies()) {
        if (body.name != "Earth") continue;
        CHECK_REL(visualRadius(body, scale), 0.55, 1e-12);
    }
}

// -------------------------------------------------------------------- picking

TEST(view_pick_hits_the_body_under_the_ray) {
    GravitySystem system;
    system.settings().pairwiseGravityEnabled = false;
    const BodyId a = system.add(named("a", 1.0, Vec3(0.0, 0.0, -10.0)));
    const BodyId b = system.add(named("b", 1.0, Vec3(6.0, 0.0, -10.0)));

    ViewScale scale = scaleFor(1.0, 1.0, 1.0);
    scale.minRadius = 0.0;

    CHECK(pickBody(system, Vec3(0.0), Vec3(0.0, 0.0, -1.0), scale, 0.0) == a);
    const Vec3 towardsB = safeNormalize(Vec3(6.0, 0.0, -10.0));
    CHECK(pickBody(system, Vec3(0.0), towardsB, scale, 0.0) == b);
}

TEST(view_pick_returns_nothing_when_the_ray_misses) {
    GravitySystem system;
    system.settings().pairwiseGravityEnabled = false;
    system.add(named("a", 1.0, Vec3(0.0, 0.0, -10.0)));

    ViewScale scale = scaleFor(1.0, 1.0, 1.0);
    scale.minRadius = 0.0;
    CHECK(pickBody(system, Vec3(0.0), Vec3(0.0, 1.0, 0.0), scale, 0.0) == kInvalidBodyId);
    CHECK(pickBody(system, Vec3(0.0), safeNormalize(Vec3(5.0, 0.0, -10.0)), scale, 0.0) ==
          kInvalidBodyId);
}

TEST(view_pick_ignores_bodies_behind_the_camera) {
    GravitySystem system;
    system.settings().pairwiseGravityEnabled = false;
    system.add(named("behind", 1.0, Vec3(0.0, 0.0, 10.0)));

    ViewScale scale = scaleFor(1.0, 1.0, 1.0);
    scale.minRadius = 0.0;
    CHECK(pickBody(system, Vec3(0.0), Vec3(0.0, 0.0, -1.0), scale, 0.0) == kInvalidBodyId);
}

TEST(view_pick_prefers_the_nearest_of_two_overlapping_bodies) {
    GravitySystem system;
    system.settings().pairwiseGravityEnabled = false;
    const BodyId near = system.add(named("near", 1.0, Vec3(0.0, 0.0, -5.0)));
    system.add(named("far", 3.0, Vec3(0.0, 0.0, -20.0)));

    ViewScale scale = scaleFor(1.0, 1.0, 1.0);
    scale.minRadius = 0.0;
    CHECK(pickBody(system, Vec3(0.0), Vec3(0.0, 0.0, -1.0), scale, 0.0) == near);
}

TEST(view_pick_uses_the_drawn_radius_not_the_physical_one) {
    // A body drawn far smaller than the click target still has to be
    // selectable: the minimum pick radius is what makes a sub-pixel planet
    // clickable at all.
    GravitySystem system;
    system.settings().pairwiseGravityEnabled = false;
    const BodyId id = system.add(named("small", 0.05, Vec3(0.0, 0.0, -10.0)));

    ViewScale scale = scaleFor(1.0, 1.0, 1.0);  // exponent 1, gain 1 -> drawn = true
    scale.minRadius = 0.0;
    CHECK_NEAR(visualRadius(*system.find(id), scale), 0.05, 1e-12);

    // A ray 0.5 units off axis at 10 units out misses a 0.05 unit sphere...
    const Vec3 offAxis = safeNormalize(Vec3(0.5, 0.0, -10.0));
    CHECK(pickBody(system, Vec3(0.0), offAxis, scale, 0.0) == kInvalidBodyId);
    // ...but hits once the pick radius is widened to a comfortable target.
    CHECK(pickBody(system, Vec3(0.0), offAxis, scale, 1.0) == id);
}

TEST(view_pick_works_at_astronomical_coordinates) {
    // The end-to-end precision check: pick a planet 1 AU from the origin with
    // the camera parked next to it.
    GravitySystem system;
    applyScene(*findScene("inner"), system);

    const CelestialBody* earth = nullptr;
    for (const CelestialBody& body : system.bodies()) {
        if (body.name == "Earth") earth = &body;
    }
    CHECK(earth != nullptr);

    const double metresPerUnit = constants::kAu / 10.0;
    ViewScale scale = scaleFor(metresPerUnit, 3.0);
    scale.minRadius = 0.05;

    const Vec3 earthUnits = earth->position / metresPerUnit;
    const Vec3 camera = earthUnits + Vec3(0.0, 0.0, 5.0);
    const Vec3 direction = safeNormalize(earthUnits - camera);

    CHECK(pickBody(system, camera, direction, scale) == earth->id);
}
