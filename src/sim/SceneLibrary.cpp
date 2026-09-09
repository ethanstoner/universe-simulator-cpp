#include "sim/SceneLibrary.h"

#include <cmath>

#include "sim/Constants.h"
#include "sim/OrbitMath.h"

namespace sim {
namespace {

using constants::kAu;
using constants::kEarthMass;
using constants::kG;
using constants::kSolarMass;

constexpr double kPi = 3.14159265358979323846;

// ---------------------------------------------------------------- body data
// Masses and mean radii are IAU/NASA fact-sheet values. Orbital radii are
// semi-major axes; the presets start every planet on a circular orbit at that
// radius rather than reproducing real eccentricities and phases, because the
// point is a legible demonstration with deterministic initial conditions.
struct PlanetData {
    const char* name;
    double mass;          // kg
    double radius;        // m
    double orbitRadius;   // m, semi-major axis
    glm::vec3 color;
};

constexpr PlanetData kMercury{"Mercury", 3.3011e23, 2.4397e6, 5.79090e10, {0.72f, 0.68f, 0.62f}};
constexpr PlanetData kVenus{"Venus", 4.8675e24, 6.0518e6, 1.08208e11, {0.94f, 0.80f, 0.52f}};
constexpr PlanetData kEarth{"Earth", 5.97219e24, 6.371e6, 1.495979e11, {0.32f, 0.55f, 0.92f}};
constexpr PlanetData kMars{"Mars", 6.4171e23, 3.3895e6, 2.27939e11, {0.85f, 0.42f, 0.28f}};
constexpr PlanetData kJupiter{"Jupiter", 1.8982e27, 6.9911e7, 7.78479e11, {0.85f, 0.72f, 0.55f}};
constexpr PlanetData kSaturn{"Saturn", 5.6834e26, 5.8232e7, 1.43353e12, {0.90f, 0.83f, 0.62f}};
constexpr PlanetData kUranus{"Uranus", 8.6810e25, 2.5362e7, 2.87096e12, {0.60f, 0.85f, 0.88f}};
constexpr PlanetData kNeptune{"Neptune", 1.02413e26, 2.4622e7, 4.49841e12, {0.30f, 0.45f, 0.85f}};

// Moon: mass, mean radius, and mean distance from Earth.
constexpr double kMoonMass = 7.342e22;
constexpr double kMoonRadius = 1.7374e6;
constexpr double kMoonOrbitRadius = 3.844e8;

constexpr double kSunRadius = 6.9634e8;

CelestialBody makeSun() {
    CelestialBody sun = makeBody("Sun", kSolarMass, kSunRadius, Vec3(0.0), Vec3(0.0),
                                 {1.00f, 0.87f, 0.42f});
    sun.emissive = true;
    sun.showTrail = false;
    return sun;
}

CelestialBody makePlanet(const PlanetData& data, const CelestialBody& primary,
                         double phase) {
    CelestialBody body = makeBody(data.name, data.mass, data.radius, Vec3(0.0), Vec3(0.0),
                                  data.color);
    placeInCircularOrbit(body, primary, data.orbitRadius, kG, phase);
    return body;
}

SimulationSettings astronomicalSettings() {
    SimulationSettings settings;
    settings.integrator = IntegratorType::VelocityVerlet;
    settings.stabilization = Stabilization::Softening;
    // 1000 km is far below any real separation in these scenes (the closest is
    // the Earth-Moon distance at 3.8e8 m) so it changes nothing observable,
    // while still bounding the force if a spawned body is dropped on top of a
    // planet.
    settings.softeningLength = 1.0e6;
    settings.minimumDistance = 1.0e6;
    settings.collisionMode = CollisionMode::Ignore;
    settings.trailLength = 1200;
    settings.bounds.enabled = false;
    settings.uniformGravity = Vec3(0.0);
    return settings;
}

SceneView innerSystemView() {
    SceneView view;
    view.metresPerUnit = kAu / 10.0;  // 1 AU = 10 world units
    // The Sun draws at 2.4 units against Mercury's 3.9 unit orbit, which is
    // exaggerated roughly 50x but keeps every inner planet clear of it.
    view.largestBodyDrawnRadius = 2.4;
    view.bodyVisualExponent = 0.35;
    view.minVisualRadius = 0.09;
    view.cameraDistance = 46.0;
    view.gridExtent = 46.0;
    view.focusBody = "Sun";
    return view;
}

// ------------------------------------------------------------------- scenes

Scene sceneKinematicsLab() {
    Scene scene;
    scene.key = "bounce";
    scene.title = "Kinematics lab (2D)";
    scene.description =
        "Uniform 9.81 m/s^2 field, no mutual gravity. Balls fall, bounce with a "
        "restitution coefficient and lose tangential speed to friction. This is "
        "the M2 demonstration and the only scene that is not astronomical.";

    scene.settings.pairwiseGravityEnabled = false;
    scene.settings.uniformGravity = Vec3(0.0, -constants::kEarthSurfaceGravity, 0.0);
    scene.settings.integrator = IntegratorType::VelocityVerlet;
    scene.settings.bounds.enabled = true;
    scene.settings.bounds.min = Vec3(-16.0, 0.0, -4.0);
    scene.settings.bounds.max = Vec3(16.0, 18.0, 4.0);
    scene.settings.bounds.restitution = 0.82;
    scene.settings.bounds.friction = 0.02;
    scene.settings.collisionMode = CollisionMode::Elastic;
    scene.settings.collisionRestitution = 0.9;
    scene.settings.trailLength = 240;

    scene.bodies.push_back(makeBody("ball-a", 1.0, 0.7, Vec3(-9.0, 16.0, 0.0),
                                    Vec3(2.4, 0.0, 0.0), {0.95f, 0.45f, 0.30f}));
    scene.bodies.push_back(makeBody("ball-b", 1.0, 0.5, Vec3(0.0, 12.0, 0.0),
                                    Vec3(-1.2, 0.0, 0.0), {0.40f, 0.75f, 0.98f}));
    scene.bodies.push_back(makeBody("ball-c", 1.0, 0.9, Vec3(8.0, 17.0, 0.0),
                                    Vec3(-3.0, 0.0, 0.0), {0.85f, 0.80f, 0.35f}));

    scene.view.metresPerUnit = 1.0;
    scene.view.trueScale = true;
    scene.view.minVisualRadius = 0.0;
    scene.view.twoDimensional = true;
    scene.view.gridEnabled = false;
    scene.view.cameraDistance = 30.0;
    scene.fixedTimeStep = 1.0 / 240.0;
    return scene;
}

Scene sceneMutualAttraction() {
    Scene scene;
    scene.key = "attract";
    scene.title = "Mutual attraction (M3)";
    scene.description =
        "Three equal masses released from rest. Nothing is scripted: they fall "
        "towards their common centre of mass purely under F = G m1 m2 / r^2.";

    scene.settings = astronomicalSettings();
    scene.settings.trailSampleInterval = 2000;
    scene.settings.softeningLength = 1.0e7;
    scene.settings.trailLength = 2000;

    const double mass = 4.0e28;
    const double radius = 3.0e8;
    const double span = 2.0e11;
    const glm::vec3 colors[3] = {
        {0.95f, 0.45f, 0.35f}, {0.40f, 0.78f, 0.98f}, {0.90f, 0.85f, 0.40f}};
    for (int i = 0; i < 3; ++i) {
        const double angle = 2.0 * kPi * i / 3.0 + kPi / 2.0;
        scene.bodies.push_back(makeBody(
            std::string("mass-") + static_cast<char>('A' + i), mass, radius,
            Vec3(span * std::cos(angle), 0.0, span * std::sin(angle)), Vec3(0.0),
            colors[i]));
    }

    scene.view.metresPerUnit = 2.0e10;
    scene.view.largestBodyDrawnRadius = 1.0;
    scene.view.bodyVisualExponent = 0.35;
    scene.view.minVisualRadius = 0.15;
    scene.view.cameraDistance = 40.0;
    scene.view.gridExtent = 40.0;
    scene.fixedTimeStep = 60.0;
    scene.defaultTimeScale = 200000.0;
    return scene;
}

Scene sceneTwoBody() {
    Scene scene;
    scene.key = "two-body";
    scene.title = "Two-body orbit (M4)";
    scene.description =
        "Earth around the Sun and nothing else. The velocity is set once from "
        "v = sqrt(GM/r) and the orbit is then produced entirely by gravity.";

    scene.settings = astronomicalSettings();
    scene.settings.trailSampleInterval = 26000;
    scene.bodies.push_back(makeSun());
    scene.bodies.push_back(makePlanet(kEarth, scene.bodies.front(), 0.0));

    scene.view = innerSystemView();
    scene.view.metresPerUnit = kAu / 10.0;
    scene.view.largestBodyDrawnRadius = 1.9;
    scene.view.cameraDistance = 32.0;
    scene.view.gridExtent = 30.0;
    scene.fixedTimeStep = 3600.0;  // one hour
    scene.defaultTimeScale = 2.0e6;
    return scene;
}

Scene sceneEarthMoonSun() {
    Scene scene;
    scene.key = "earth-moon";
    scene.title = "Sun, Earth and Moon";
    scene.description =
        "Viewed at Earth-Moon scale: one world unit is 50 000 km, so the "
        "lunar orbit is about 7.7 units across. The Sun is still present and "
        "still the dominant mass, 2 992 units away off screen -- the Moon's "
        "orbit is being computed in its field, not in isolation. Both bodies "
        "swing about their common barycentre, which sits inside the Earth.";

    scene.settings = astronomicalSettings();
    scene.settings.trailSampleInterval = 800;
    scene.settings.trailLength = 3000;
    scene.bodies.push_back(makeSun());

    CelestialBody earth = makePlanet(kEarth, scene.bodies.front(), 0.0);
    scene.bodies.push_back(earth);

    CelestialBody moon = makeBody("Moon", kMoonMass, kMoonRadius, Vec3(0.0), Vec3(0.0),
                                  {0.78f, 0.78f, 0.76f});
    placeInCircularOrbit(moon, earth, kMoonOrbitRadius, kG, 0.0);
    scene.bodies.push_back(moon);

    scene.view = innerSystemView();
    // 1 unit = 50 000 km. At the AU scale used by the other presets the entire
    // lunar orbit would be a quarter of the Earth's own drawn radius.
    scene.view.metresPerUnit = 5.0e7;
    scene.view.scaleReferenceBody = "Earth";
    scene.view.largestBodyDrawnRadius = 0.55;
    scene.view.bodyVisualExponent = 0.35;
    scene.view.minVisualRadius = 0.05;
    scene.view.cameraDistance = 26.0;
    scene.view.gridExtent = 26.0;
    scene.view.focusBody = "Earth";
    scene.view.trailReferenceBody = "Earth";
    scene.fixedTimeStep = 600.0;  // ten minutes; the Moon's period is 27.3 days
    scene.defaultTimeScale = 3.0e5;
    return scene;
}

Scene sceneInnerSystem() {
    Scene scene;
    scene.key = "inner";
    scene.title = "Inner solar system";
    scene.description =
        "Sun, Mercury, Venus, Earth, Moon and Mars, from real masses and "
        "semi-major axes, each started on a circular orbit.";

    scene.settings = astronomicalSettings();
    scene.settings.trailSampleInterval = 50000;
    scene.bodies.push_back(makeSun());
    const CelestialBody sun = scene.bodies.front();

    scene.bodies.push_back(makePlanet(kMercury, sun, 0.4));
    scene.bodies.push_back(makePlanet(kVenus, sun, 2.1));
    CelestialBody earth = makePlanet(kEarth, sun, 0.0);
    scene.bodies.push_back(earth);

    CelestialBody moon = makeBody("Moon", kMoonMass, kMoonRadius, Vec3(0.0), Vec3(0.0),
                                  {0.78f, 0.78f, 0.76f});
    placeInCircularOrbit(moon, earth, kMoonOrbitRadius, kG, 0.0);
    scene.bodies.push_back(moon);

    scene.bodies.push_back(makePlanet(kMars, sun, 3.6));

    scene.view = innerSystemView();
    scene.fixedTimeStep = 1800.0;
    scene.defaultTimeScale = 2.0e6;
    return scene;
}

Scene sceneSolarSystem() {
    Scene scene;
    scene.key = "solar-system";
    scene.title = "Solar system with Jupiter";
    scene.description =
        "Sun through Neptune. Body radii are exaggerated by a large factor; at "
        "true scale the Earth would be far under one pixel across.";

    scene.settings = astronomicalSettings();
    scene.settings.trailSampleInterval = 150000;
    scene.settings.trailLength = 2400;
    scene.bodies.push_back(makeSun());
    const CelestialBody sun = scene.bodies.front();

    scene.bodies.push_back(makePlanet(kMercury, sun, 0.4));
    scene.bodies.push_back(makePlanet(kVenus, sun, 2.1));
    CelestialBody earth = makePlanet(kEarth, sun, 0.0);
    scene.bodies.push_back(earth);
    CelestialBody moon = makeBody("Moon", kMoonMass, kMoonRadius, Vec3(0.0), Vec3(0.0),
                                  {0.78f, 0.78f, 0.76f});
    placeInCircularOrbit(moon, earth, kMoonOrbitRadius, kG, 0.0);
    scene.bodies.push_back(moon);
    scene.bodies.push_back(makePlanet(kMars, sun, 3.6));
    scene.bodies.push_back(makePlanet(kJupiter, sun, 1.2));
    scene.bodies.push_back(makePlanet(kSaturn, sun, 4.4));
    scene.bodies.push_back(makePlanet(kUranus, sun, 5.6));
    scene.bodies.push_back(makePlanet(kNeptune, sun, 2.9));

    scene.view.metresPerUnit = kAu / 2.2;  // Neptune at 30 AU -> ~66 units
    // A higher exponent than the inner-system presets: this view has to hold
    // the Sun and Mercury's 0.85 unit orbit in one frame, so the size ratio
    // must stay closer to reality or Mercury is drawn larger than its orbit.
    scene.view.largestBodyDrawnRadius = 3.4;
    scene.view.bodyVisualExponent = 0.42;
    scene.view.minVisualRadius = 0.09;
    scene.view.cameraDistance = 110.0;
    scene.view.gridExtent = 140.0;
    scene.view.focusBody = "Sun";
    scene.fixedTimeStep = 7200.0;
    scene.defaultTimeScale = 1.0e7;
    return scene;
}

Scene sceneBinaryStars() {
    Scene scene;
    scene.key = "binary";
    scene.title = "Binary stars";
    scene.description =
        "Two solar-mass stars orbiting their common barycentre at 0.5 AU "
        "separation, with a planet on a wide circumbinary orbit.";

    scene.settings = astronomicalSettings();
    scene.settings.trailSampleInterval = 15000;
    scene.settings.trailLength = 2400;

    const double separation = 0.5 * kAu;
    const double starMass = kSolarMass;
    const double totalMass = 2.0 * starMass;
    // Each star sits at half the separation from the barycentre and moves at
    // half the relative orbital speed.
    const double relativeSpeed = std::sqrt(kG * totalMass / separation);

    CelestialBody a = makeBody("Star A", starMass, kSunRadius,
                               Vec3(-separation / 2.0, 0.0, 0.0),
                               Vec3(0.0, 0.0, -relativeSpeed / 2.0),
                               {1.00f, 0.82f, 0.40f});
    a.emissive = true;
    CelestialBody b = makeBody("Star B", starMass, kSunRadius * 0.9,
                               Vec3(separation / 2.0, 0.0, 0.0),
                               Vec3(0.0, 0.0, relativeSpeed / 2.0),
                               {0.70f, 0.82f, 1.00f});
    b.emissive = true;
    scene.bodies.push_back(a);
    scene.bodies.push_back(b);

    // Circumbinary planet: far enough out that the pair looks like a point mass.
    CelestialBody planet = makeBody("Circumbinary", kEarthMass, kEarth.radius, Vec3(0.0),
                                    Vec3(0.0), {0.40f, 0.85f, 0.60f});
    const double planetRadius = 4.0 * kAu;
    planet.position = Vec3(planetRadius, 0.0, 0.0);
    planet.velocity = Vec3(0.0, 0.0, std::sqrt(kG * totalMass / planetRadius));
    scene.bodies.push_back(planet);

    scene.view.metresPerUnit = kAu / 6.0;
    scene.view.largestBodyDrawnRadius = 1.5;
    scene.view.bodyVisualExponent = 0.35;
    scene.view.minVisualRadius = 0.10;
    scene.view.cameraDistance = 70.0;
    scene.view.gridExtent = 55.0;
    scene.fixedTimeStep = 1800.0;
    scene.defaultTimeScale = 2.0e6;
    return scene;
}

Scene sceneThreeBody() {
    Scene scene;
    scene.key = "three-body";
    scene.title = "Three-body chaos";
    scene.description =
        "Three equal-mass stars on a rotating equilateral triangle, the "
        "Lagrange solution to the three-body problem, with one mass nudged 2% "
        "heavier. That solution is linearly unstable for equal masses, so it "
        "holds for several orbits and then breaks up, usually ejecting a star. "
        "The three-body problem has no general closed-form solution, but the "
        "integrator is deterministic: identical initial conditions reproduce "
        "exactly, while a one-metre nudge diverges macroscopically.";

    scene.settings = astronomicalSettings();
    scene.settings.trailSampleInterval = 100000;
    scene.settings.trailLength = 3000;
    scene.settings.softeningLength = 1.0e8;

    const double mass = kSolarMass;
    const double radius = 1.5 * kAu;

    // Speed for a rotating equilateral triangle of three equal masses.
    //
    // Each star sits at circumradius r, so the other two are r*sqrt(3) away.
    // Each pulls with G m^2 / (3 r^2), and the two pulls resolve towards the
    // centre as 2 cos(30 deg) times that, giving G m^2 / (sqrt(3) r^2). Setting
    // that equal to the centripetal requirement m v^2 / r leaves
    //     v = sqrt(G m / (sqrt(3) r)).
    //
    // The previous value here was 0.9 * sqrt(G M / r), the circular speed about
    // a single central mass, which is simply the wrong problem: it is far too
    // fast and the triangle tore itself apart inside two years instead of
    // orbiting. The Lagrange triangle for equal masses is still linearly
    // unstable, so it does eventually break up -- but over many orbits, which
    // is the behaviour actually worth watching.
    const double speed = std::sqrt(kG * mass / (std::sqrt(3.0) * radius));
    const glm::vec3 colors[3] = {
        {1.00f, 0.75f, 0.45f}, {0.65f, 0.80f, 1.00f}, {1.00f, 0.55f, 0.55f}};

    for (int i = 0; i < 3; ++i) {
        const double angle = 2.0 * kPi * i / 3.0;
        CelestialBody star = makeBody(
            std::string("Star ") + static_cast<char>('A' + i), mass, kSunRadius,
            Vec3(radius * std::cos(angle), 0.0, radius * std::sin(angle)),
            // Tangential, giving the triangle a net rotation.
            Vec3(-speed * std::sin(angle), 0.0, speed * std::cos(angle)), colors[i]);
        star.emissive = true;
        scene.bodies.push_back(star);
    }
    // A small deliberate asymmetry. Without it the triangle is an exact
    // solution and floating-point noise alone decides when it destabilises,
    // which makes the moment of breakup arbitrary rather than reproducible.
    scene.bodies[2].mass *= 1.02;

    scene.view.metresPerUnit = kAu / 4.0;
    // The stars sit 10.4 units apart, so 1.3 keeps them clearly separate.
    scene.view.largestBodyDrawnRadius = 1.3;
    scene.view.bodyVisualExponent = 0.35;
    scene.view.minVisualRadius = 0.12;
    scene.view.cameraDistance = 80.0;
    scene.view.gridExtent = 70.0;
    scene.fixedTimeStep = 3600.0;
    scene.defaultTimeScale = 4.0e6;
    return scene;
}

Scene sceneIntruderStar() {
    Scene scene = sceneInnerSystem();
    scene.key = "intruder";
    scene.settings.trailSampleInterval = 5.0e4;
    scene.title = "Intruder star";
    scene.description =
        "The inner solar system with a 0.8 solar-mass star falling in from 12 AU "
        "on a grazing trajectory. The planets' orbits are disrupted by nothing "
        "but the extra term in the same pairwise sum.";

    CelestialBody intruder = makeBody("Intruder", 0.8 * kSolarMass, kSunRadius * 0.8,
                                      Vec3(-12.0 * kAu, 0.0, 6.0 * kAu),
                                      Vec3(11000.0, 0.0, -6000.0),
                                      {1.00f, 0.55f, 0.35f});
    intruder.emissive = true;
    scene.bodies.push_back(intruder);

    scene.view.metresPerUnit = kAu / 4.0;
    scene.view.cameraDistance = 52.0;
    scene.view.gridExtent = 62.0;
    scene.view.largestBodyDrawnRadius = 1.5;
    scene.view.bodyVisualExponent = 0.4;
    scene.fixedTimeStep = 1800.0;
    scene.defaultTimeScale = 4.0e6;
    return scene;
}

Scene sceneCompactObject() {
    Scene scene;
    scene.key = "compact";
    scene.title = "Compact massive object";
    scene.description =
        "A 12 solar-mass compact object with a tight ring of test bodies. This "
        "is a NEWTONIAN point mass, not a black hole: the simulation still uses "
        "F = G m1 m2 / r^2 with no relativistic terms. Its Schwarzschild radius "
        "is reported for reference and drawn as a marker sphere, but nothing "
        "special happens at that radius.";

    scene.settings = astronomicalSettings();
    scene.settings.trailSampleInterval = 15;
    scene.settings.trailLength = 2000;
    // The interesting orbits here come within a few million km, so the
    // softening length must be well below that or it would change the physics
    // being demonstrated rather than just guarding against a singularity.
    scene.settings.softeningLength = 1.0e5;
    scene.settings.stabilization = Stabilization::Softening;

    const double mass = 12.0 * kSolarMass;
    CelestialBody compact = makeBody("Compact object", mass,
                                     schwarzschildRadius(mass) * 3.0, Vec3(0.0),
                                     Vec3(0.0), {0.10f, 0.05f, 0.18f});
    compact.showTrail = false;
    scene.bodies.push_back(compact);

    // Six probes on circular orbits at increasing radii, so the r^-3/2 period
    // scaling is directly visible as differential rotation.
    for (int i = 0; i < 6; ++i) {
        const double orbitRadius = (0.02 + 0.022 * i) * kAu;
        CelestialBody probe = makeBody(
            "Probe " + std::to_string(i + 1), 1.0e22, 4.0e6, Vec3(0.0), Vec3(0.0),
            {0.45f + 0.09f * i, 0.75f - 0.05f * i, 1.0f});
        placeInCircularOrbit(probe, scene.bodies.front(), orbitRadius, kG,
                             i * 1.05);
        scene.bodies.push_back(probe);
    }

    scene.view.metresPerUnit = kAu / 120.0;
    // The compact object's own radius is metres, not millions of kilometres,
    // so the power law is what makes it visible at all here.
    scene.view.largestBodyDrawnRadius = 1.0;
    scene.view.bodyVisualExponent = 0.35;
    scene.view.minVisualRadius = 0.10;
    scene.view.cameraDistance = 45.0;
    scene.view.gridExtent = 34.0;
    scene.view.focusBody = "Compact object";
    scene.fixedTimeStep = 20.0;
    scene.defaultTimeScale = 3.0e4;
    return scene;
}

Scene sceneCompactNearSun() {
    Scene scene;
    scene.key = "compact-vs-sun";
    scene.title = "Compact mass meets the solar system";
    scene.description =
        "A 30 solar-mass compact object passes through the inner solar system. "
        "Still Newtonian; see docs/PHYSICS.md for what this is and is not.";

    scene.settings = astronomicalSettings();
    scene.settings.trailSampleInterval = 50000;
    scene.settings.trailLength = 2400;
    scene.settings.softeningLength = 1.0e6;

    scene.bodies.push_back(makeSun());
    const CelestialBody sun = scene.bodies.front();
    scene.bodies.push_back(makePlanet(kMercury, sun, 0.4));
    scene.bodies.push_back(makePlanet(kVenus, sun, 2.1));
    scene.bodies.push_back(makePlanet(kEarth, sun, 0.0));
    scene.bodies.push_back(makePlanet(kMars, sun, 3.6));
    scene.bodies.push_back(makePlanet(kJupiter, sun, 1.2));

    const double mass = 30.0 * kSolarMass;
    CelestialBody compact = makeBody("Compact object", mass,
                                     schwarzschildRadius(mass) * 4.0,
                                     Vec3(-9.0 * kAu, 0.0, 3.5 * kAu),
                                     Vec3(16000.0, 0.0, -6000.0),
                                     {0.12f, 0.06f, 0.20f});
    scene.bodies.push_back(compact);

    scene.view.metresPerUnit = kAu / 3.0;
    scene.view.largestBodyDrawnRadius = 2.0;
    scene.view.bodyVisualExponent = 0.42;
    scene.view.minVisualRadius = 0.10;
    scene.view.cameraDistance = 50.0;
    scene.view.gridExtent = 60.0;
    scene.fixedTimeStep = 1800.0;
    scene.defaultTimeScale = 4.0e6;
    return scene;
}

std::vector<Scene> buildScenes() {
    std::vector<Scene> scenes;
    scenes.push_back(sceneKinematicsLab());
    scenes.push_back(sceneMutualAttraction());
    scenes.push_back(sceneTwoBody());
    scenes.push_back(sceneEarthMoonSun());
    scenes.push_back(sceneInnerSystem());
    scenes.push_back(sceneSolarSystem());
    scenes.push_back(sceneBinaryStars());
    scenes.push_back(sceneThreeBody());
    scenes.push_back(sceneIntruderStar());
    scenes.push_back(sceneCompactObject());
    scenes.push_back(sceneCompactNearSun());

    // Every astronomical preset is built from heliocentric velocities, which
    // give the whole system a net drift. Remove it so scenes stay put.
    for (Scene& scene : scenes) {
        if (!scene.settings.pairwiseGravityEnabled) continue;
        zeroNetMomentum(scene.bodies);
    }
    return scenes;
}

}  // namespace

CelestialBody makeBody(const std::string& name, double mass, double radius,
                       const Vec3& position, const Vec3& velocity,
                       const glm::vec3& color) {
    CelestialBody body;
    body.name = name;
    body.mass = mass;
    body.radius = radius;
    body.position = position;
    body.velocity = velocity;
    body.color = color;
    return body;
}

void placeInCircularOrbit(CelestialBody& body, const CelestialBody& primary,
                          double radius, double G, double phaseRadians) {
    const Vec3 offset(radius * std::cos(phaseRadians), 0.0,
                      radius * std::sin(phaseRadians));
    body.position = primary.position + offset;

    // Two-body circular speed uses the total mass. For a planet round the Sun
    // the difference is one part in 3e5, but for the Earth-Moon pair it is
    // over 1%.
    const double speed = std::sqrt(G * (primary.mass + body.mass) / radius);
    const Vec3 tangent(-std::sin(phaseRadians), 0.0, std::cos(phaseRadians));
    body.velocity = primary.velocity + tangent * speed;
}

void zeroNetMomentum(std::vector<CelestialBody>& bodies) {
    Vec3 momentum(0.0);
    double totalMass = 0.0;
    for (const CelestialBody& body : bodies) {
        momentum += body.momentum();
        totalMass += body.mass;
    }
    if (totalMass <= 0.0) return;
    const Vec3 drift = momentum / totalMass;
    for (CelestialBody& body : bodies) body.velocity -= drift;
}

void recentreOnBarycentre(std::vector<CelestialBody>& bodies) {
    Vec3 weighted(0.0);
    double totalMass = 0.0;
    for (const CelestialBody& body : bodies) {
        weighted += body.position * body.mass;
        totalMass += body.mass;
    }
    if (totalMass <= 0.0) return;
    const Vec3 centre = weighted / totalMass;
    for (CelestialBody& body : bodies) body.position -= centre;
}

namespace {
std::vector<Scene>& registry() {
    static std::vector<Scene> scenes = buildScenes();
    return scenes;
}
}  // namespace

const std::vector<BodyPreset>& bodyPresets() {
    static const std::vector<BodyPreset> presets = {
        {"Moon", kMoonMass, kMoonRadius, {0.78f, 0.78f, 0.76f}, false, ""},
        {"Earth", kEarth.mass, kEarth.radius, kEarth.color, false, ""},
        {"Jupiter", kJupiter.mass, kJupiter.radius, kJupiter.color, false, ""},
        {"Sun", kSolarMass, kSunRadius, {1.00f, 0.87f, 0.42f}, true, ""},
        {"Red dwarf", 0.25 * kSolarMass, 0.3 * kSunRadius, {1.00f, 0.45f, 0.32f}, true,
         "A 0.25 solar-mass star."},
        {"White dwarf", 0.9 * kSolarMass, 6.4e6, {0.85f, 0.92f, 1.00f}, true,
         "A Sun-like mass compressed to roughly Earth's radius."},
        {"Neutron-star-like", 1.4 * kSolarMass, 1.2e4, {0.80f, 0.88f, 1.00f}, true,
         "1.4 solar masses in a 12 km ball. Newtonian only: no relativistic "
         "structure, degeneracy pressure or frame dragging is modelled."},
        {"Compact massive object", 10.0 * kSolarMass, 3.0e4, {0.10f, 0.05f, 0.18f}, false,
         "A 10 solar-mass point mass. NOT a black hole: this simulator has no "
         "event horizon and no relativistic terms. Its Schwarzschild radius is "
         "reported for reference only."},
    };
    return presets;
}

const BodyPreset* findBodyPreset(const std::string& name) {
    for (const BodyPreset& preset : bodyPresets()) {
        if (preset.name == name) return &preset;
    }
    return nullptr;
}

const std::vector<Scene>& builtinScenes() { return registry(); }

void registerScene(Scene scene) {
    std::vector<Scene>& scenes = registry();
    for (Scene& existing : scenes) {
        if (existing.key == scene.key) {
            existing = std::move(scene);
            return;
        }
    }
    scenes.push_back(std::move(scene));
}

const Scene* findScene(const std::string& key) {
    for (const Scene& scene : builtinScenes()) {
        if (scene.key == key) return &scene;
    }
    return nullptr;
}

std::vector<std::string> sceneKeys() {
    std::vector<std::string> keys;
    for (const Scene& scene : builtinScenes()) keys.push_back(scene.key);
    return keys;
}

void applyScene(const Scene& scene, GravitySystem& system) {
    system.clear();
    system.settings() = scene.settings;
    for (const CelestialBody& body : scene.bodies) {
        CelestialBody copy = body;
        copy.id = kInvalidBodyId;  // let the system assign fresh ids
        copy.trail.clear();
        system.add(copy);
    }
}

}  // namespace sim
