#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "sim/CelestialBody.h"
#include "sim/Constants.h"
#include "sim/Integrator.h"
#include "sim/Vec.h"

namespace sim {

// How the 1/r^2 singularity is kept from blowing up when two bodies approach.
// Whichever is chosen, it is a documented modification of Newtonian gravity,
// not a silent fudge -- see docs/PHYSICS.md.
enum class Stabilization {
    None,         // raw 1/r^2. Will produce infinities on a direct hit.
    Softening,    // Plummer softening: a = G m r / (r^2 + eps^2)^(3/2)
    MinDistance,  // clamp the separation used in the denominator to a floor
};

enum class CollisionMode {
    Ignore,   // bodies pass through one another
    Elastic,  // impulse exchange with a restitution coefficient
    Merge,    // combine into one body, conserving mass and momentum
};

const char* stabilizationName(Stabilization mode);
const char* collisionModeName(CollisionMode mode);

// An axis-aligned box the bodies bounce inside. Used by the kinematics lab
// (M2); disabled for astronomical scenes.
struct Bounds {
    bool enabled = false;
    Vec3 min{-10.0, 0.0, -10.0};
    Vec3 max{10.0, 20.0, 10.0};
    double restitution = 0.8;  // 1 = perfectly elastic, 0 = fully inelastic
    double friction = 0.0;     // tangential velocity lost per bounce, 0..1
};

struct SimulationSettings {
    double gravitationalConstant = constants::kG;
    IntegratorType integrator = IntegratorType::VelocityVerlet;

    bool pairwiseGravityEnabled = true;
    Stabilization stabilization = Stabilization::Softening;
    double softeningLength = 1.0e6;   // m -- ~0.16 Earth radii, negligible at AU scales
    double minimumDistance = 1.0e6;   // m, used when stabilization is MinDistance

    // A uniform field added to every body. Zero for astronomical scenes; set to
    // (0, -9.81, 0) for the Earth-surface kinematics demonstration.
    Vec3 uniformGravity{0.0};

    Bounds bounds;

    CollisionMode collisionMode = CollisionMode::Ignore;
    double collisionRestitution = 0.5;
    // Bodies merge/collide when their centres come within
    // (r1 + r2) * collisionRadiusScale. Real planetary radii are tiny compared
    // with orbital separations, so a scale of 1 means contact almost never
    // happens by accident.
    double collisionRadiusScale = 1.0;

    std::size_t trailLength = 900;      // samples retained per body
    double trailSampleInterval = 0.0;   // simulated seconds between samples; 0 = every step
};

// Records what a step did, so the UI can report merges rather than having
// bodies silently vanish.
struct StepReport {
    int merges = 0;
    int elasticCollisions = 0;
    int boundaryBounces = 0;
};

class GravitySystem : public ForceModel {
public:
    GravitySystem();

    // ------------------------------------------------------------- body list
    BodyId add(CelestialBody body);   // assigns an id if the body has none
    bool remove(BodyId id);
    void clear();

    CelestialBody* find(BodyId id);
    const CelestialBody* find(BodyId id) const;

    std::vector<CelestialBody>& bodies() { return bodies_; }
    const std::vector<CelestialBody>& bodies() const { return bodies_; }
    std::size_t size() const { return bodies_.size(); }

    // ------------------------------------------------------------- stepping
    // Advances by exactly `dt` seconds. dt is a *fixed* physics step; time
    // acceleration is expressed as more calls, never as a larger dt.
    StepReport step(double dt);

    double elapsedSimulatedSeconds() const { return elapsed_; }
    void resetElapsed() { elapsed_ = 0.0; }
    unsigned long long stepCount() const { return steps_; }

    // ForceModel: a(x) for every body, from pairwise gravity plus the uniform
    // field. Pure in `positions`; masses come from the body list.
    void accelerations(const std::vector<Vec3>& positions,
                       std::vector<Vec3>& out) const override;

    // Acceleration a single test particle of negligible mass would feel at
    // `point`. Used by the spacetime-grid preview and by orbit prediction; it
    // does not affect the simulation.
    Vec3 accelerationAt(const Vec3& point, BodyId ignore = kInvalidBodyId) const;

    SimulationSettings& settings() { return settings_; }
    const SimulationSettings& settings() const { return settings_; }

    void clearTrails();
    // Call after mutating body state from outside so cached accelerations and
    // the trail sampler are consistent again.
    void invalidate();

    BodyId nextId() const { return nextId_; }
    void setNextId(BodyId id) { nextId_ = id; }

private:
    void syncToState();
    void syncFromState();
    void applyBounds(StepReport& report);
    void applyCollisions(double dt, StepReport& report);
    void recordTrails();

    std::vector<CelestialBody> bodies_;
    SimulationSettings settings_;

    // Structure-of-arrays working state handed to the integrator.
    std::vector<Vec3> positions_;
    std::vector<Vec3> velocities_;
    std::vector<Vec3> accelerations_;

    double elapsed_ = 0.0;
    double lastTrailSample_ = 0.0;
    unsigned long long steps_ = 0;
    BodyId nextId_ = 1;
    bool primed_ = false;
};

}  // namespace sim
