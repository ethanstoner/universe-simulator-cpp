#include "TestFramework.h"

#include <cmath>
#include <set>

#include "sim/Constants.h"
#include "sim/Diagnostics.h"
#include "sim/GravitySystem.h"
#include "sim/OrbitMath.h"
#include "sim/SceneLibrary.h"

using namespace sim;

namespace {

const CelestialBody* byName(const GravitySystem& system, const std::string& name) {
    for (const CelestialBody& body : system.bodies()) {
        if (body.name == name) return &body;
    }
    return nullptr;
}

}  // namespace

TEST(scenes_all_presets_are_well_formed) {
    const std::vector<Scene>& scenes = builtinScenes();
    CHECK(scenes.size() >= 8);

    std::set<std::string> keys;
    for (const Scene& scene : scenes) {
        CHECK(!scene.key.empty());
        CHECK(!scene.title.empty());
        CHECK(!scene.description.empty());
        CHECK(!scene.bodies.empty());
        CHECK(scene.fixedTimeStep > 0.0);
        CHECK(scene.view.metresPerUnit > 0.0);
        CHECK(keys.insert(scene.key).second);  // keys are unique

        for (const CelestialBody& body : scene.bodies) {
            CHECK(!body.name.empty());
            CHECK(body.mass >= 0.0);
            CHECK(body.radius >= 0.0);
            CHECK(std::isfinite(body.position.x));
            CHECK(std::isfinite(body.velocity.x));
        }
    }
}

TEST(scenes_apply_replaces_bodies_and_settings) {
    GravitySystem system;
    const Scene* inner = findScene("inner");
    CHECK(inner != nullptr);
    applyScene(*inner, system);

    CHECK(system.size() == inner->bodies.size());
    CHECK(system.elapsedSimulatedSeconds() == 0.0);
    CHECK(byName(system, "Sun") != nullptr);
    CHECK(byName(system, "Earth") != nullptr);
    CHECK(byName(system, "Moon") != nullptr);
    CHECK(byName(system, "Mars") != nullptr);

    // Ids must be unique and non-zero after loading.
    std::set<BodyId> ids;
    for (const CelestialBody& body : system.bodies()) {
        CHECK(body.id != kInvalidBodyId);
        CHECK(ids.insert(body.id).second);
    }
}

TEST(scenes_astronomical_presets_have_no_net_momentum) {
    for (const Scene& scene : builtinScenes()) {
        if (!scene.settings.pairwiseGravityEnabled) continue;
        GravitySystem system;
        applyScene(scene, system);

        const SystemDiagnostics d = computeDiagnostics(system);
        double scale = 0.0;
        for (const CelestialBody& body : system.bodies()) {
            scale += glm::length(body.momentum());
        }
        if (scale == 0.0) {
            // "attract" releases every mass from rest, so there is no momentum
            // to normalise against and the total must be identically zero.
            CHECK(d.linearMomentum == Vec3(0.0));
            continue;
        }
        CHECK_LESS(glm::length(d.linearMomentum) / scale, 1e-14);
    }
}

TEST(scenes_earth_orbits_the_sun_at_the_right_speed) {
    GravitySystem system;
    applyScene(*findScene("two-body"), system);

    const CelestialBody* sun = byName(system, "Sun");
    const CelestialBody* earth = byName(system, "Earth");
    CHECK(sun != nullptr && earth != nullptr);

    const double distance = glm::length(earth->position - sun->position);
    const double relativeSpeed = glm::length(earth->velocity - sun->velocity);
    CHECK_NEAR(distance / constants::kAu, 1.0, 0.005);
    CHECK_NEAR(relativeSpeed, 29784.0, 60.0);
}

TEST(scenes_earth_completes_one_orbit_in_one_year) {
    // The strongest end-to-end check available: run the actual integrator for
    // one Julian year of simulated time and require the Earth to come back to
    // where it started.
    GravitySystem system;
    const Scene* scene = findScene("two-body");
    applyScene(*scene, system);

    const CelestialBody* earth = byName(system, "Earth");
    const Vec3 start = earth->position;
    const double startAngle = std::atan2(start.z, start.x);

    const double dt = 3600.0;  // one hour
    const int steps = static_cast<int>(constants::kJulianYear / dt);
    for (int i = 0; i < steps; ++i) system.step(dt);

    earth = byName(system, "Earth");
    const double endAngle = std::atan2(earth->position.z, earth->position.x);
    double delta = std::abs(endAngle - startAngle);
    if (delta > 3.14159265358979) delta = 2.0 * 3.14159265358979 - delta;

    // Within about a degree of a full revolution. It is not exact because the
    // orbit is initialised as a perfect circle at the semi-major axis, whereas
    // a real year corresponds to a slightly eccentric orbit.
    CHECK_LESS(delta, 0.02);

    // And the orbital radius must not have wandered.
    const double radius = glm::length(earth->position);
    CHECK_NEAR(radius / constants::kAu, 1.0, 0.005);
}

TEST(scenes_inner_system_orbits_stay_bounded_for_a_decade) {
    GravitySystem system;
    applyScene(*findScene("inner"), system);

    struct Initial {
        std::string name;
        double radius;
    };
    std::vector<Initial> initial;
    const CelestialBody* sun = byName(system, "Sun");
    for (const CelestialBody& body : system.bodies()) {
        if (body.name == "Sun" || body.name == "Moon") continue;
        initial.push_back({body.name, glm::length(body.position - sun->position)});
    }
    CHECK(!initial.empty());

    const double dt = 1800.0;
    const int steps = static_cast<int>(10.0 * constants::kJulianYear / dt);
    for (int i = 0; i < steps; ++i) system.step(dt);

    sun = byName(system, "Sun");
    for (const Initial& entry : initial) {
        const CelestialBody* body = byName(system, entry.name);
        CHECK(body != nullptr);
        const double radius = glm::length(body->position - sun->position);
        // Circular initial conditions plus real mutual perturbations: a few
        // percent of radial variation is expected, a factor of two is not.
        CHECK_LESS(std::abs(radius - entry.radius) / entry.radius, 0.05);
    }
}

TEST(scenes_moon_stays_bound_to_the_earth) {
    GravitySystem system;
    applyScene(*findScene("earth-moon"), system);

    const double dt = 600.0;
    const int steps = static_cast<int>(2.0 * constants::kJulianYear / dt);
    double minDistance = 1e30, maxDistance = 0.0;
    for (int i = 0; i < steps; ++i) {
        system.step(dt);
        if (i % 20 != 0) continue;
        const CelestialBody* earth = byName(system, "Earth");
        const CelestialBody* moon = byName(system, "Moon");
        const double distance = glm::length(moon->position - earth->position);
        minDistance = std::min(minDistance, distance);
        maxDistance = std::max(maxDistance, distance);
    }
    // The real Moon's distance varies between about 3.63e8 and 4.06e8 m. A
    // circular start perturbed by the Sun should stay in that neighbourhood.
    CHECK(minDistance > 3.0e8);
    CHECK(maxDistance < 4.6e8);
}

TEST(scenes_energy_drift_over_a_decade_of_the_inner_system) {
    GravitySystem system;
    applyScene(*findScene("inner"), system);

    const double reference = computeDiagnostics(system).totalEnergy;
    CHECK(reference < 0.0);  // a bound system has negative total energy

    const double dt = 1800.0;
    const int steps = static_cast<int>(10.0 * constants::kJulianYear / dt);
    double peak = 0.0;
    for (int i = 0; i < steps; ++i) {
        system.step(dt);
        if (i % 500 != 0) continue;
        const double energy = computeDiagnostics(system).totalEnergy;
        peak = std::max(peak, std::abs((energy - reference) / reference));
    }
    CHECK_LESS(peak, 1e-5);
}

TEST(scenes_compact_object_probes_are_on_bound_orbits) {
    GravitySystem system;
    applyScene(*findScene("compact"), system);

    const CelestialBody* compact = byName(system, "Compact object");
    CHECK(compact != nullptr);
    const double mu = constants::kG * compact->mass;

    int probes = 0;
    for (const CelestialBody& body : system.bodies()) {
        if (body.name == "Compact object") continue;
        ++probes;
        const OrbitalElements elements = computeOrbitalElements(
            body.position - compact->position, body.velocity - compact->velocity, mu);
        CHECK(elements.bound);
        CHECK_LESS(elements.eccentricity, 0.02);
    }
    CHECK(probes == 6);
}

TEST(scenes_compact_object_schwarzschild_radius_is_far_below_the_orbits) {
    // The point of the honesty caveat: the interesting orbits are nowhere near
    // the Schwarzschild radius, so nothing relativistic is being approximated.
    GravitySystem system;
    applyScene(*findScene("compact"), system);
    const CelestialBody* compact = byName(system, "Compact object");
    const double rs = schwarzschildRadius(compact->mass);

    for (const CelestialBody& body : system.bodies()) {
        if (body.name == "Compact object") continue;
        const double radius = glm::length(body.position - compact->position);
        CHECK(radius > 1000.0 * rs);
    }
}

TEST(scenes_intruder_star_actually_disrupts_the_inner_system) {
    // M11: adding another star must change existing orbits through gravity, not
    // through any special-cased code. Compare the same planets with and without
    // the intruder over the same simulated interval.
    auto radiusOfMars = [](const std::string& key, double seconds) {
        GravitySystem system;
        applyScene(*findScene(key), system);
        const double dt = 1800.0;
        const int steps = static_cast<int>(seconds / dt);
        for (int i = 0; i < steps; ++i) system.step(dt);
        const CelestialBody* sun = byName(system, "Sun");
        const CelestialBody* mars = byName(system, "Mars");
        return glm::length(mars->position - sun->position);
    };

    const double years = 12.0 * constants::kJulianYear;
    const double undisturbed = radiusOfMars("inner", years);
    const double disturbed = radiusOfMars("intruder", years);
    // The intruder passes through; Mars' distance from the Sun must differ by
    // far more than the few-percent variation of the undisturbed case.
    CHECK(std::abs(disturbed - undisturbed) / undisturbed > 0.10);
}

TEST(scenes_three_body_is_deterministic_but_chaotic) {
    auto run = [](double perturbation) {
        GravitySystem system;
        applyScene(*findScene("three-body"), system);
        system.bodies()[0].position.x += perturbation;
        system.invalidate();
        const double dt = 3600.0;
        const int steps = static_cast<int>(30.0 * constants::kJulianYear / dt);
        for (int i = 0; i < steps; ++i) system.step(dt);
        return system.bodies()[0].position;
    };

    // Identical inputs reproduce exactly.
    const Vec3 a = run(0.0);
    const Vec3 b = run(0.0);
    CHECK(a.x == b.x && a.y == b.y && a.z == b.z);

    // A one metre nudge to a body 1.5 AU out diverges macroscopically: that is
    // the chaos, and it is a property of the system, not a bug.
    const Vec3 c = run(1.0);
    CHECK(glm::length(c - a) > 1.0e9);
}
