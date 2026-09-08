#pragma once

#include <vector>

#include "sim/CelestialBody.h"

namespace sim {

struct SimulationSettings;

struct CollisionOutcome {
    int merges = 0;
    int elasticCollisions = 0;
};

// Resolves overlaps in place. Merging removes the absorbed body from `bodies`,
// so callers must not hold pointers into the vector across this call.
//
// The model is deliberately crude and its limitations are documented in
// docs/PHYSICS.md: contact is treated as instantaneous, bodies are rigid
// spheres, no tidal disruption or fragmentation is modelled, and a merge
// conserves mass and linear momentum but discards the kinetic energy of the
// relative motion (as heat that is never tracked).
CollisionOutcome resolveCollisions(std::vector<CelestialBody>& bodies,
                                   const SimulationSettings& settings);

// Merges `other` into `into`: mass adds, momentum is conserved, and the radius
// is recomputed from the summed volume. Exposed for unit testing.
void mergeInto(CelestialBody& into, const CelestialBody& other);

}  // namespace sim
