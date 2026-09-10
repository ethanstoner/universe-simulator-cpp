#include "TestFramework.h"

#include <cmath>
#include <cstdio>
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

TEST(scenes_astronomical_orbits_are_prograde_about_plus_y) {
    // Every scene lays its orbits out in the XZ plane and +Y is that plane's
    // normal, so an orbit that turns the way the real solar system turns as
    // seen from ecliptic north has its angular momentum along +Y. This is not
    // cosmetic: computeOrbitalElements measures inclination against +Y, so a
    // retrograde layout reports every coplanar orbit as inclined 180 degrees.
    const char* keys[] = {"two-body", "earth-moon", "inner",   "solar-system",
                          "binary",   "compact",    "belt",    "trojans",
                          "precession"};

    for (const char* key : keys) {
        const Scene* scene = findScene(key);
        CHECK(scene != nullptr);
        if (!scene) continue;

        const CelestialBody* primary = &scene->bodies.front();
        for (const CelestialBody& body : scene->bodies) {
            if (body.mass > primary->mass) primary = &body;
        }

        for (const CelestialBody& body : scene->bodies) {
            if (&body == primary) continue;
            const Vec3 angularMomentum =
                glm::cross(body.position - primary->position,
                           body.velocity - primary->velocity);
            CHECK(angularMomentum.y > 0.0);
        }
    }
}

TEST(scenes_coplanar_presets_report_zero_inclination) {
    // The reading the Bodies inspector shows. A coplanar preset must come out
    // at 0 degrees, not 180.
    GravitySystem system;
    applyScene(*findScene("inner"), system);

    const CelestialBody* sun = byName(system, "Sun");
    CHECK(sun != nullptr);
    for (const char* name : {"Mercury", "Venus", "Earth", "Mars"}) {
        const CelestialBody* planet = byName(system, name);
        CHECK(planet != nullptr);
        if (!planet) continue;
        const double mu = constants::kG * (sun->mass + planet->mass);
        const OrbitalElements elements =
            computeOrbitalElements(planet->position - sun->position,
                                   planet->velocity - sun->velocity, mu);
        CHECK(elements.valid);
        CHECK_NEAR(elements.inclination, 0.0, 1e-12);
    }
}

TEST(scenes_spawn_and_preset_orbits_turn_the_same_way) {
    // The spawn panel's "auto orbit" runs through circularOrbitVelocity with
    // +Y as the orbit normal; the presets run through placeInCircularOrbit.
    // If the two disagree, a body spawned into a scene counter-orbits
    // everything already in it.
    CelestialBody primary = makeBody("primary", constants::kSolarMass, 6.96e8,
                                     Vec3(0.0), Vec3(0.0), glm::vec3(1.0f));
    CelestialBody body =
        makeBody("body", 1.0, 1.0, Vec3(0.0), Vec3(0.0), glm::vec3(1.0f));

    for (double phase : {0.0, 0.7, 2.9, -1.4}) {
        placeInCircularOrbit(body, primary, constants::kAu, constants::kG, phase);
        const Vec3 spawned =
            circularOrbitVelocity(primary.position, primary.mass, body.position,
                                  Vec3(0.0, 1.0, 0.0), constants::kG);
        CHECK(glm::dot(safeNormalize(body.velocity), safeNormalize(spawned)) > 0.999);
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

// --------------------------------------------------- M12: extreme compact mass

TEST(compact_mass_dropped_on_a_star_stays_numerically_finite) {
    // The worst case the UI allows: a 1.4 solar-mass, 12 km object released at
    // rest and allowed to fall straight through the Sun. Without stabilisation
    // this is a division by a vanishing r; with it, everything must stay finite
    // and the energy must stay bounded.
    GravitySystem system;
    applyScene(*findScene("inner"), system);

    CelestialBody compact = makeBody("Compact", 1.4 * constants::kSolarMass, 1.2e4,
                                     Vec3(0.0, 0.0, 3.0 * constants::kAu), Vec3(0.0),
                                     glm::vec3(0.2f));
    // Aimed almost exactly at the Sun, so the closest approach is tiny.
    compact.position.x = 1.0e6;
    system.add(compact);

    const double reference = std::abs(computeDiagnostics(system).totalEnergy);
    double closestApproach = 1e30;

    const double dt = 600.0;
    for (int i = 0; i < 200000; ++i) {
        system.step(dt);
        const CelestialBody* sun = byName(system, "Sun");
        const CelestialBody* body = byName(system, "Compact");
        if (!sun || !body) break;
        closestApproach =
            std::min(closestApproach, glm::length(body->position - sun->position));

        for (const CelestialBody& each : system.bodies()) {
            CHECK(std::isfinite(each.position.x));
            CHECK(std::isfinite(each.position.y));
            CHECK(std::isfinite(each.position.z));
            CHECK(std::isfinite(each.velocity.x));
            CHECK(std::isfinite(each.speed()));
        }
    }

    // It really did come close, so the guard was actually exercised.
    CHECK_LESS(closestApproach, 5.0e9);
    // And nothing acquired a superluminal speed, which is the usual symptom of
    // a singularity being hit.
    for (const CelestialBody& body : system.bodies()) {
        CHECK_LESS(body.speed(), constants::kC);
    }
    const double energy = std::abs(computeDiagnostics(system).totalEnergy);
    CHECK(std::isfinite(energy));
    (void)reference;
}

TEST(compact_mass_encounter_error_is_timestep_resolution_not_a_broken_force) {
    // The close pass above does NOT conserve energy well: at a 600 s step the
    // total moves by a factor of thousands. That is worth being precise about,
    // because "the softening is hiding an explosion" and "the step is too
    // coarse to resolve the encounter" look identical from a single run.
    //
    // They are distinguishable by refining the step. Truncation error at an
    // unresolved encounter shrinks as the step shrinks; a broken force law or a
    // singularity being papered over does not. So this measures the drift at
    // three step sizes and requires it to fall.
    auto driftAtStep = [](double dt) {
        GravitySystem system;
        system.settings().stabilization = Stabilization::Softening;
        system.settings().softeningLength = 1.0e6;
        system.settings().trailLength = 0;

        // A two-body close pass, which keeps the measurement clean. The impact
        // parameter is chosen so that periapsis lands near 3.6e9 m, where the
        // characteristic time is about 8500 s: coarse at a 600 s step and
        // comfortably resolved at 37.5 s. A tighter pass is not a useful test,
        // because if NO tested step resolves the encounter the error is
        // chaotic rather than convergent and the comparison means nothing.
        system.add(makeBody("star", constants::kSolarMass, 6.96e8, Vec3(0.0), Vec3(0.0),
                            glm::vec3(1.0f)));
        CelestialBody compact =
            makeBody("compact", 1.4 * constants::kSolarMass, 1.2e4,
                     Vec3(5.0e10, 0.0, 2.0e11), Vec3(0.0, 0.0, -3.0e4), glm::vec3(0.2f));
        system.add(compact);
        zeroNetMomentum(system.bodies());
        system.invalidate();

        const double reference = computeDiagnostics(system).totalEnergy;
        const double totalTime = 4.0e7;
        const long long steps = static_cast<long long>(totalTime / dt);
        for (long long i = 0; i < steps; ++i) system.step(dt);

        for (const CelestialBody& body : system.bodies()) {
            CHECK(std::isfinite(body.position.x));
            CHECK_LESS(body.speed(), constants::kC);
        }
        const double energy = computeDiagnostics(system).totalEnergy;
        return std::abs((energy - reference) / reference);
    };

    const double coarse = driftAtStep(600.0);
    const double medium = driftAtStep(150.0);
    const double fine = driftAtStep(37.5);

    // Every refinement must improve matters, and the finest must be far better
    // than the coarsest. If softening were masking a singularity these would
    // all be equally bad.
    CHECK_LESS(medium, coarse);
    CHECK_LESS(fine, medium);
    CHECK_LESS(fine, coarse / 10.0);
}

TEST(unstabilised_gravity_really_does_blow_up) {
    // The counterpart: with stabilisation off, a direct hit produces exactly
    // the infinity the softening exists to prevent. Documented rather than
    // hidden -- if this ever stops failing, the stabilisation tests above are
    // no longer proving anything.
    GravitySystem system;
    system.settings().stabilization = Stabilization::None;
    system.settings().trailLength = 0;

    CelestialBody a = makeBody("a", constants::kSolarMass, 1.0, Vec3(0.0), Vec3(0.0),
                               glm::vec3(1.0f));
    CelestialBody b = makeBody("b", 1.0e20, 1.0, Vec3(1.0e-4, 0.0, 0.0), Vec3(0.0),
                               glm::vec3(1.0f));
    system.add(a);
    system.add(b);

    for (int i = 0; i < 200; ++i) system.step(1.0);
    const double speed = system.bodies()[1].speed();
    CHECK(speed > constants::kC);  // nonsense, as expected without softening

    // The same setup with softening stays sane.
    GravitySystem guarded;
    guarded.settings().stabilization = Stabilization::Softening;
    guarded.settings().softeningLength = 1.0e6;
    guarded.settings().trailLength = 0;
    guarded.add(a);
    guarded.add(b);
    for (int i = 0; i < 200; ++i) guarded.step(1.0);
    CHECK_LESS(guarded.bodies()[1].speed(), constants::kC);
}

TEST(three_body_triangle_uses_the_lagrange_speed) {
    // For three equal masses on an equilateral triangle of circumradius r the
    // balance is v = sqrt(G m / (sqrt(3) r)). Getting this wrong -- the preset
    // originally used the circular speed about a single central mass -- makes
    // the configuration fly apart in under two years instead of orbiting.
    GravitySystem system;
    applyScene(*findScene("three-body"), system);
    CHECK(system.size() == 3);

    const CelestialBody& star = system.bodies()[0];
    const double radius = glm::length(star.position);
    const double expected =
        std::sqrt(constants::kG * constants::kSolarMass / (std::sqrt(3.0) * radius));
    // Within 2%: zeroNetMomentum shifts velocities slightly because one mass
    // was nudged heavier.
    CHECK_NEAR(glm::length(star.velocity) / expected, 1.0, 0.02);
}

TEST(three_body_triangle_survives_several_orbits_before_breaking_up) {
    GravitySystem system;
    applyScene(*findScene("three-body"), system);

    const double radius = glm::length(system.bodies()[0].position);
    const double speed = glm::length(system.bodies()[0].velocity);
    const double period = 2.0 * 3.14159265358979 * radius / speed;

    // Four orbits in, the stars must still be a bound cluster rather than
    // scattered debris.
    const double dt = 3600.0;
    const int steps = static_cast<int>(4.0 * period / dt);
    for (int i = 0; i < steps; ++i) system.step(dt);

    double furthest = 0.0;
    for (const CelestialBody& body : system.bodies()) {
        furthest = std::max(furthest, glm::length(body.position));
    }
    CHECK_LESS(furthest, radius * 4.0);
    CHECK(computeDiagnostics(system).totalEnergy < 0.0);  // still bound
}

// --------------------------------------------- Lagrange points and Trojans

TEST(trojans_stay_librating_around_l4_and_l5) {
    // The claim the scene makes is that nothing pins the asteroids to the
    // Lagrange points and they stay anyway. That is only worth saying if it is
    // true, so this measures the angle between each asteroid and Jupiter, in
    // Jupiter's rotating frame, over a century of simulated time.
    GravitySystem system;
    applyScene(*findScene("trojans"), system);

    auto angleTo = [](const Vec3& v) { return std::atan2(v.z, v.x); };
    auto wrap = [](double angle) {
        while (angle > 3.14159265358979) angle -= 2.0 * 3.14159265358979;
        while (angle < -3.14159265358979) angle += 2.0 * 3.14159265358979;
        return angle;
    };

    const CelestialBody* jupiter = byName(system, "Jupiter");
    CHECK(jupiter != nullptr);

    // Jupiter's period is 11.86 years; run for roughly nine orbits.
    const double dt = 7200.0;
    const int steps = static_cast<int>(107.0 * constants::kJulianYear / dt);

    double worstLeading = 0.0;
    double worstTrailing = 0.0;

    for (int i = 0; i < steps; ++i) {
        system.step(dt);
        if (i % 500 != 0) continue;

        const CelestialBody* sun = byName(system, "Sun");
        jupiter = byName(system, "Jupiter");
        const double jupiterAngle = angleTo(jupiter->position - sun->position);

        for (const CelestialBody& body : system.bodies()) {
            if (body.name.rfind("L4-", 0) != 0 && body.name.rfind("L5-", 0) != 0) {
                continue;
            }
            const double separation =
                wrap(angleTo(body.position - sun->position) - jupiterAngle);
            // Orbits travel towards decreasing phase angle, so L4 -- the
            // leading point -- sits 60 degrees BELOW Jupiter's angle.
            const bool leading = body.name.rfind("L4-", 0) == 0;
            const double target = leading ? -3.14159265358979 / 3.0
                                          : 3.14159265358979 / 3.0;
            const double excursion = std::abs(wrap(separation - target));
            if (leading) worstLeading = std::max(worstLeading, excursion);
            else worstTrailing = std::max(worstTrailing, excursion);
        }
    }

    const double toDegrees = 180.0 / 3.14159265358979;
    std::printf("         Trojan libration: L4 within %.1f deg, L5 within %.1f deg "
                "of the Lagrange point after ~9 Jupiter orbits\n",
                worstLeading * toDegrees, worstTrailing * toDegrees);

    // Tadpole libration around L4/L5 is tens of degrees wide; escaping the
    // point entirely would drift right around the orbit. The distinction that
    // matters is bounded versus unbounded.
    CHECK_LESS(worstLeading * toDegrees, 60.0);
    CHECK_LESS(worstTrailing * toDegrees, 60.0);
}

TEST(trojans_would_not_stay_at_an_arbitrary_angle) {
    // The control: L4 and L5 are special. Placing the same swarm 120 degrees
    // ahead instead of 60 must NOT hold, otherwise the previous test is
    // measuring "co-rotating bodies stay put" rather than Lagrange stability.
    GravitySystem system;
    applyScene(*findScene("trojans"), system);

    const CelestialBody* sun = byName(system, "Sun");
    const CelestialBody* jupiter = byName(system, "Jupiter");
    const double radius = glm::length(jupiter->position - sun->position);
    const double angularSpeed =
        std::sqrt(constants::kG * constants::kSolarMass / radius) / radius;

    // One test particle at 120 degrees ahead, co-rotating exactly. Ahead means
    // negative phase, the same way L4 does.
    const double angle = -2.0 * 3.14159265358979 / 3.0;
    CelestialBody probe = makeBody("Probe", 1.0e17, 3.0e5,
                                   Vec3(radius * std::cos(angle), 0.0,
                                        radius * std::sin(angle)),
                                   Vec3(0.0), glm::vec3(1.0f));
    const double speed = angularSpeed * radius;
    probe.velocity = progradeTangent(angle) * speed;
    const BodyId probeId = system.add(probe);

    auto angleTo = [](const Vec3& v) { return std::atan2(v.z, v.x); };
    auto wrap = [](double a) {
        while (a > 3.14159265358979) a -= 2.0 * 3.14159265358979;
        while (a < -3.14159265358979) a += 2.0 * 3.14159265358979;
        return a;
    };

    const double dt = 7200.0;
    const int steps = static_cast<int>(107.0 * constants::kJulianYear / dt);
    double worst = 0.0;
    for (int i = 0; i < steps; ++i) {
        system.step(dt);
        if (i % 500 != 0) continue;
        const CelestialBody* s = byName(system, "Sun");
        const CelestialBody* j = byName(system, "Jupiter");
        const CelestialBody* p = system.find(probeId);
        if (!s || !j || !p) break;
        const double separation =
            wrap(angleTo(p->position - s->position) - angleTo(j->position - s->position));
        worst = std::max(worst, std::abs(wrap(separation - angle)));
    }

    const double toDegrees = 180.0 / 3.14159265358979;
    std::printf("         Non-Lagrange probe at 120 deg drifted %.1f deg\n",
                worst * toDegrees);
    // It drifts far further than the Trojans do, which is the whole point.
    CHECK(worst * toDegrees > 60.0);
}
