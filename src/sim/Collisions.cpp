#include "sim/Collisions.h"

#include <algorithm>
#include <cmath>

#include "sim/GravitySystem.h"

namespace sim {
namespace {
constexpr double kPi = 3.14159265358979323846;
}

void mergeInto(CelestialBody& into, const CelestialBody& other) {
    const double totalMass = into.mass + other.mass;
    if (totalMass <= 0.0) return;

    // Momentum conservation: v = (m1 v1 + m2 v2) / (m1 + m2).
    const Vec3 momentum = into.momentum() + other.momentum();
    // Centre of mass, so the merged body does not jump to the heavier one's
    // centre and inject spurious potential energy.
    const Vec3 centreOfMass =
        (into.position * into.mass + other.position * other.mass) / totalMass;

    // Volumes add, which keeps mean density between the two originals rather
    // than letting the result be arbitrarily dense.
    const double volume = (4.0 / 3.0) * kPi *
                          (into.radius * into.radius * into.radius +
                           other.radius * other.radius * other.radius);

    into.position = centreOfMass;
    into.mass = totalMass;
    into.velocity = momentum / totalMass;
    into.radius = std::cbrt(volume * 3.0 / (4.0 * kPi));
    into.acceleration = Vec3(0.0);  // recomputed on the next force evaluation
    into.trail.clear();
    if (other.emissive) into.emissive = true;
}

CollisionOutcome resolveCollisions(std::vector<CelestialBody>& bodies,
                                   const SimulationSettings& settings) {
    CollisionOutcome outcome;
    if (settings.collisionMode == CollisionMode::Ignore) return outcome;

    const double scale = settings.collisionRadiusScale;

    for (std::size_t i = 0; i < bodies.size(); ++i) {
        for (std::size_t j = i + 1; j < bodies.size();) {
            CelestialBody& a = bodies[i];
            CelestialBody& b = bodies[j];

            const Vec3 delta = b.position - a.position;
            const double contact = (a.radius + b.radius) * scale;
            const double distanceSq = lengthSquared(delta);
            if (distanceSq >= contact * contact || contact <= 0.0) {
                ++j;
                continue;
            }

            if (settings.collisionMode == CollisionMode::Merge) {
                // The heavier body survives and keeps its identity, which is
                // what a viewer expects when a pebble hits the Sun.
                if (b.mass > a.mass) {
                    CelestialBody survivor = b;
                    mergeInto(survivor, a);
                    survivor.id = b.id;
                    survivor.name = b.name;
                    a = survivor;
                } else {
                    mergeInto(a, b);
                }
                bodies.erase(bodies.begin() + static_cast<long>(j));
                ++outcome.merges;
                continue;  // re-test body i against the new j
            }

            // Elastic: exchange impulse along the contact normal and push the
            // pair apart so they do not stay interpenetrating and re-collide
            // every step.
            const double distance = std::sqrt(distanceSq);
            const Vec3 normal = distance > 0.0 ? delta / distance : Vec3(1.0, 0.0, 0.0);
            const Vec3 relative = b.velocity - a.velocity;
            const double approach = glm::dot(relative, normal);

            if (approach < 0.0) {  // only if they are actually closing
                const double inverseMassA = a.fixed || a.mass <= 0.0 ? 0.0 : 1.0 / a.mass;
                const double inverseMassB = b.fixed || b.mass <= 0.0 ? 0.0 : 1.0 / b.mass;
                const double inverseMassSum = inverseMassA + inverseMassB;
                if (inverseMassSum > 0.0) {
                    const double impulse = -(1.0 + settings.collisionRestitution) *
                                           approach / inverseMassSum;
                    a.velocity -= normal * (impulse * inverseMassA);
                    b.velocity += normal * (impulse * inverseMassB);

                    const double overlap = contact - distance;
                    a.position -= normal * (overlap * inverseMassA / inverseMassSum);
                    b.position += normal * (overlap * inverseMassB / inverseMassSum);
                }
                ++outcome.elasticCollisions;
            }
            ++j;
        }
    }
    return outcome;
}

}  // namespace sim
