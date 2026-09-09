#include "sim/Trajectory.h"

#include <algorithm>

#include "sim/GravitySystem.h"

namespace sim {

Trajectory predictTrajectory(const GravitySystem& system,
                             const TrajectoryRequest& request) {
    Trajectory result;
    result.body = request.body;

    if (request.body == kInvalidBodyId) return result;
    if (request.horizonSeconds <= 0.0 || request.stepSeconds <= 0.0) return result;
    if (!system.find(request.body)) return result;

    // A full copy, so the forecast cannot disturb the live simulation. This is
    // the expensive part on a large scene, which is why the caller decides how
    // often to ask for one.
    GravitySystem scratch = system;

    // Collisions are off in the forecast. A merge would delete the body being
    // predicted and truncate the path halfway, which reads as a bug rather
    // than as the collision it actually is.
    scratch.settings().collisionMode = CollisionMode::Ignore;
    scratch.settings().trailLength = 0;  // never read here, and it costs memory
    scratch.invalidate();

    const int maxSamples = std::clamp(request.maxSamples, 2, 20000);
    const int stepsPerSample = std::max(request.stepsPerSample, 1);

    auto samplePosition = [&]() -> Vec3 {
        const CelestialBody* body = scratch.find(request.body);
        if (!body) return Vec3(0.0);
        if (request.referenceBody == kInvalidBodyId) return body->position;
        const CelestialBody* reference = scratch.find(request.referenceBody);
        return reference ? body->position - reference->position : body->position;
    };

    result.points.reserve(static_cast<std::size_t>(maxSamples));
    result.points.push_back(samplePosition());

    double elapsed = 0.0;
    while (elapsed < request.horizonSeconds &&
           static_cast<int>(result.points.size()) < maxSamples) {
        for (int i = 0; i < stepsPerSample; ++i) {
            scratch.step(request.stepSeconds);
            elapsed += request.stepSeconds;
        }
        if (!scratch.find(request.body)) break;  // vanished; stop cleanly
        result.points.push_back(samplePosition());
    }

    result.horizonSeconds = elapsed;
    // Truncated means the sample cap was reached before the requested horizon,
    // so the drawn path is shorter than asked for rather than wrong.
    result.truncated = elapsed < request.horizonSeconds;
    return result;
}

}  // namespace sim
