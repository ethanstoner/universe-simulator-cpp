#pragma once

#include <vector>

#include "sim/CelestialBody.h"
#include "sim/Vec.h"

namespace sim {

class GravitySystem;

// Predicted future path of a single body.
//
// This is a *forecast*, not a Keplerian ellipse. The body is advanced through a
// copy of the real system with the real integrator, so the prediction includes
// mutual perturbation from every other body and matches what will actually
// happen -- right up until something the forecast could not know about changes
// the system, such as a body being spawned or an orbit being edited.
//
// The trajectory is therefore recomputed whenever the system changes, and is
// explicitly labelled in the UI as a prediction under current conditions.
struct Trajectory {
    BodyId body = kInvalidBodyId;
    std::vector<Vec3> points;   // world positions, in the chosen frame
    double horizonSeconds = 0.0;  // simulated time actually covered
    bool truncated = false;       // hit the sample cap before the horizon
};

struct TrajectoryRequest {
    BodyId body = kInvalidBodyId;
    double horizonSeconds = 0.0;   // how far ahead to predict
    int maxSamples = 600;          // points retained
    // Frame the result is expressed in. kInvalidBodyId means inertial, which is
    // right for a planet round the Sun; naming a body gives the relative path,
    // which is the only way to see a moon's loop.
    BodyId referenceBody = kInvalidBodyId;
    // Steps per sample. Larger integrates faster and more coarsely; the
    // prediction uses the system's own fixed timestep as the unit.
    int stepsPerSample = 1;
    // Simulated seconds per integration step. Should be the system's own
    // fixed timestep so the forecast follows the same discretised path the
    // live simulation will take.
    double stepSeconds = 0.0;
};

// Integrates a copy of `system` forward and returns the predicted path.
// The original system is never modified.
Trajectory predictTrajectory(const GravitySystem& system,
                             const TrajectoryRequest& request);

}  // namespace sim
