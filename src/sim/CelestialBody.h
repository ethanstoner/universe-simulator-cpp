#pragma once

#include <cstdint>
#include <deque>
#include <string>

#include <glm/vec3.hpp>

#include "sim/Vec.h"

namespace sim {

using BodyId = std::uint32_t;
inline constexpr BodyId kInvalidBodyId = 0;

// One gravitating body. All physical quantities are SI.
//
// `radius` is the real physical radius and is what collision and merge tests
// use. `renderRadiusScale` is a *display* multiplier and is never read by any
// physics code -- see docs/ARCHITECTURE.md "Scales".
struct CelestialBody {
    BodyId id = kInvalidBodyId;
    std::string name;

    Vec3 position{0.0};      // m
    Vec3 velocity{0.0};      // m/s
    Vec3 acceleration{0.0};  // m/s^2, from the most recent force evaluation

    double mass = 0.0;    // kg
    double radius = 0.0;  // m, physical mean radius

    // Display-only.
    double renderRadiusScale = 1.0;
    glm::vec3 color{1.0f};
    bool emissive = false;  // stars: drawn unlit and used as a light source
    bool showTrail = true;

    // A fixed body still attracts others but is never moved by them. Useful for
    // pinning the Sun so a scene does not drift out of view.
    bool fixed = false;

    // Trail history is bounded; see GravitySystem::trailLength.
    std::deque<Vec3> trail;

    double kineticEnergy() const { return 0.5 * mass * lengthSquared(velocity); }
    double speed() const { return glm::length(velocity); }
    Vec3 momentum() const { return velocity * mass; }

    // Mean density, kg/m^3. Returns 0 for a point mass.
    double density() const;

    // Sets the radius so the body has the given density at its current mass.
    void setRadiusFromDensity(double density);
};

}  // namespace sim
