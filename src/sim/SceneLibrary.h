#pragma once

#include <string>
#include <vector>

#include "sim/CelestialBody.h"
#include "sim/GravitySystem.h"

namespace sim {

// Hints the renderer uses when a scene is loaded. This is plain data -- `sim/`
// still has no idea what a camera or an OpenGL context is -- but a scene has to
// be able to say "this one is 1e12 m across" so the view is not left staring at
// an empty volume.
struct SceneView {
    // Metres per world unit. Positions are divided by this before being drawn.
    double metresPerUnit = 1.0;
    // Radius, in world units, that the scene's LARGEST body should draw at.
    // The renderer solves its power-law gain from this, so every other body
    // follows automatically. See RenderSettings for why a constant multiplier
    // does not work at astronomical scale.
    double largestBodyDrawnRadius = 1.2;
    // Body whose drawn radius should equal largestBodyDrawnRadius. Empty means
    // "whichever body is physically largest", which is right for scenes framed
    // on a star but wrong for a close-up: the Earth-Moon preset still contains
    // the Sun, 2 992 units off screen, and solving the gain from it made the
    // Earth six pixels across.
    std::string scaleReferenceBody;
    // Exponent of the power-law radius exaggeration. Lower values flatter the
    // size differences (good when every body is a similar size); higher values
    // preserve them (needed when a scene spans the Sun and Mercury together).
    double bodyVisualExponent = 0.35;
    // Draw bodies at their true relative size. Only sensible when the scene's
    // objects are of comparable size, as in the kinematics lab.
    bool trueScale = false;
    // A floor on drawn radius, in world units, so tiny bodies stay clickable.
    double minVisualRadius = 0.02;
    double cameraDistance = 40.0;   // world units
    double cameraPitchDegrees = 28.0;
    bool twoDimensional = false;    // draw with an orthographic top-down view
    bool gridEnabled = true;
    double gridExtent = 60.0;       // world units, half-width of the grid
    std::string focusBody;          // name of the body the camera starts on
    // Body whose frame trails are recorded in; empty means the inertial frame.
    // Needed for close-up scenes: at Earth-Moon scale the Earth travels 833
    // world units in a fortnight, so an inertial trail is a streak off screen.
    std::string trailReferenceBody;
};

struct Scene {
    std::string key;
    std::string title;
    std::string description;
    SimulationSettings settings;
    std::vector<CelestialBody> bodies;
    SceneView view;
    // Physics step this scene wants, in simulated seconds. A solar system needs
    // a much larger step than a bouncing ball.
    double fixedTimeStep = 1.0 / 120.0;
    double defaultTimeScale = 1.0;
};

// All available presets, in menu order: the built-in ones plus anything
// registered from configs/*.json at startup.
const std::vector<Scene>& builtinScenes();

// Adds a scene, or replaces the existing one with the same key. This is how a
// JSON file in configs/ takes precedence over the compiled-in definition, so
// masses and orbital radii can be edited without a rebuild.
void registerScene(Scene scene);
const Scene* findScene(const std::string& key);
std::vector<std::string> sceneKeys();

// Applies a scene to a system: replaces the bodies and settings and resets the
// clock. Does not touch anything render-side.
void applyScene(const Scene& scene, GravitySystem& system);

// Shifts every body into the barycentric frame so the system as a whole does
// not drift off screen. Real ephemeris velocities are heliocentric, which gives
// the whole solar system a net momentum.
void zeroNetMomentum(std::vector<CelestialBody>& bodies);
void recentreOnBarycentre(std::vector<CelestialBody>& bodies);

// Helpers used by the presets and by the interactive spawn UI.
CelestialBody makeBody(const std::string& name, double mass, double radius,
                       const Vec3& position, const Vec3& velocity,
                       const glm::vec3& color);

// Places `body` on a circular orbit of `radius` around `primary`, in the XZ
// plane. The velocity is derived from sqrt(G(M+m)/r), never assigned by hand.
void placeInCircularOrbit(CelestialBody& body, const CelestialBody& primary,
                          double radius, double G, double phaseRadians = 0.0);

}  // namespace sim
