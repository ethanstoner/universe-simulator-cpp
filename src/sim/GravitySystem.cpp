#include "sim/GravitySystem.h"

#include <algorithm>
#include <cmath>

#include "sim/Collisions.h"

namespace sim {

const char* stabilizationName(Stabilization mode) {
    switch (mode) {
        case Stabilization::None: return "None (raw 1/r^2)";
        case Stabilization::Softening: return "Plummer softening";
        case Stabilization::MinDistance: return "Minimum distance clamp";
    }
    return "?";
}

const char* gravitySolverName(GravitySolver solver) {
    switch (solver) {
        case GravitySolver::Direct: return "Direct O(N^2)";
        case GravitySolver::BarnesHut: return "Barnes-Hut O(N log N)";
    }
    return "?";
}

const char* collisionModeName(CollisionMode mode) {
    switch (mode) {
        case CollisionMode::Ignore: return "Ignore";
        case CollisionMode::Elastic: return "Elastic";
        case CollisionMode::Merge: return "Merge";
    }
    return "?";
}

GravitySystem::GravitySystem() = default;

BodyId GravitySystem::add(CelestialBody body) {
    if (body.id == kInvalidBodyId) body.id = nextId_++;
    else nextId_ = std::max(nextId_, body.id + 1);
    bodies_.push_back(std::move(body));
    invalidate();
    return bodies_.back().id;
}

bool GravitySystem::remove(BodyId id) {
    const auto it = std::find_if(bodies_.begin(), bodies_.end(),
                                 [id](const CelestialBody& b) { return b.id == id; });
    if (it == bodies_.end()) return false;
    bodies_.erase(it);
    invalidate();
    return true;
}

void GravitySystem::clear() {
    bodies_.clear();
    positions_.clear();
    velocities_.clear();
    accelerations_.clear();
    elapsed_ = 0.0;
    lastTrailSample_ = 0.0;
    steps_ = 0;
    nextId_ = 1;
    primed_ = false;
}

CelestialBody* GravitySystem::find(BodyId id) {
    for (CelestialBody& body : bodies_) {
        if (body.id == id) return &body;
    }
    return nullptr;
}

const CelestialBody* GravitySystem::find(BodyId id) const {
    return const_cast<GravitySystem*>(this)->find(id);
}

void GravitySystem::invalidate() { primed_ = false; }

void GravitySystem::clearTrails() {
    for (CelestialBody& body : bodies_) body.trail.clear();
}

void GravitySystem::syncToState() {
    const std::size_t n = bodies_.size();
    positions_.resize(n);
    velocities_.resize(n);
    accelerations_.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        positions_[i] = bodies_[i].position;
        velocities_[i] = bodies_[i].velocity;
        accelerations_[i] = bodies_[i].acceleration;
    }
}

void GravitySystem::syncFromState() {
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        bodies_[i].position = positions_[i];
        bodies_[i].velocity = velocities_[i];
        bodies_[i].acceleration = accelerations_[i];
    }
}

void GravitySystem::accelerations(const std::vector<Vec3>& positions,
                                  std::vector<Vec3>& out) const {
    const std::size_t n = positions.size();
    out.assign(n, settings_.uniformGravity);
    if (!settings_.pairwiseGravityEnabled || n < 2) {
        // Fixed bodies never move, whatever the field says.
        for (std::size_t i = 0; i < n && i < bodies_.size(); ++i) {
            if (bodies_[i].fixed) out[i] = Vec3(0.0);
        }
        return;
    }

    const double G = settings_.gravitationalConstant;
    const double softeningSq = settings_.softeningLength * settings_.softeningLength;
    const double minDistanceSq = settings_.minimumDistance * settings_.minimumDistance;

    if (settings_.solver == GravitySolver::BarnesHut) {
        // The tree only supports Plummer-style softening, which is the same
        // constant added to r^2 that the direct path uses. MinDistance clamps
        // per pair and has no aggregate equivalent, so it degrades to the
        // softening term here; None passes zero.
        double treeSoftening = 0.0;
        if (settings_.stabilization == Stabilization::Softening) {
            treeSoftening = softeningSq;
        } else if (settings_.stabilization == Stabilization::MinDistance) {
            treeSoftening = minDistanceSq;
        }

        treeMasses_.resize(n);
        for (std::size_t i = 0; i < n && i < bodies_.size(); ++i) {
            treeMasses_[i] = bodies_[i].mass;
        }
        tree_.build(positions, treeMasses_);

        for (std::size_t i = 0; i < n; ++i) {
            if (i < bodies_.size() && bodies_[i].fixed) continue;
            out[i] += tree_.accelerationAt(positions[i], static_cast<int>(i), G,
                                           settings_.barnesHutTheta, treeSoftening);
        }
        for (std::size_t i = 0; i < n && i < bodies_.size(); ++i) {
            if (bodies_[i].fixed) out[i] = Vec3(0.0);
        }
        return;
    }

    // Each unordered pair is visited once and the equal-and-opposite pair of
    // accelerations is applied together (Newton's third law), which halves the
    // work and keeps total momentum exactly conserved up to rounding.
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) {
            const Vec3 delta = positions[j] - positions[i];  // i -> j
            double distanceSq = lengthSquared(delta);

            switch (settings_.stabilization) {
                case Stabilization::None:
                    break;
                case Stabilization::Softening:
                    distanceSq += softeningSq;
                    break;
                case Stabilization::MinDistance:
                    distanceSq = std::max(distanceSq, minDistanceSq);
                    break;
            }

            if (distanceSq <= 0.0) continue;  // exactly coincident and unsoftened

            // a = G m delta / |delta|^3, written as delta * (G m / d^3) to avoid
            // a normalize() plus a separate square.
            const double inverseDistanceCubed = 1.0 / (distanceSq * std::sqrt(distanceSq));
            const double scale = G * inverseDistanceCubed;
            out[i] += delta * (scale * bodies_[j].mass);
            out[j] -= delta * (scale * bodies_[i].mass);
        }
    }

    for (std::size_t i = 0; i < n && i < bodies_.size(); ++i) {
        if (bodies_[i].fixed) out[i] = Vec3(0.0);
    }
}

Vec3 GravitySystem::accelerationAt(const Vec3& point, BodyId ignore) const {
    Vec3 total = settings_.uniformGravity;
    const double G = settings_.gravitationalConstant;
    const double softeningSq = settings_.softeningLength * settings_.softeningLength;
    const double minDistanceSq = settings_.minimumDistance * settings_.minimumDistance;

    for (const CelestialBody& body : bodies_) {
        if (body.id == ignore) continue;
        const Vec3 delta = body.position - point;
        double distanceSq = lengthSquared(delta);
        switch (settings_.stabilization) {
            case Stabilization::None: break;
            case Stabilization::Softening: distanceSq += softeningSq; break;
            case Stabilization::MinDistance:
                distanceSq = std::max(distanceSq, minDistanceSq);
                break;
        }
        if (distanceSq <= 0.0) continue;
        total += delta * (G * body.mass / (distanceSq * std::sqrt(distanceSq)));
    }
    return total;
}

StepReport GravitySystem::step(double dt) {
    StepReport report;
    if (bodies_.empty() || dt == 0.0) return report;

    syncToState();
    if (!primed_) {
        primeAccelerations(*this, positions_, accelerations_);
        primed_ = true;
    }

    integrate(settings_.integrator, *this, dt, positions_, velocities_, accelerations_);
    syncFromState();

    applyBounds(report);
    applyCollisions(dt, report);

    elapsed_ += dt;
    ++steps_;
    recordTrails();
    return report;
}

void GravitySystem::applyBounds(StepReport& report) {
    const Bounds& bounds = settings_.bounds;
    if (!bounds.enabled) return;

    for (CelestialBody& body : bodies_) {
        if (body.fixed) continue;
        for (int axis = 0; axis < 3; ++axis) {
            const double low = bounds.min[axis] + body.radius;
            const double high = bounds.max[axis] - body.radius;
            bool hit = false;

            if (body.position[axis] < low) {
                body.position[axis] = low;
                if (body.velocity[axis] < 0.0) hit = true;
            } else if (body.position[axis] > high) {
                body.position[axis] = high;
                if (body.velocity[axis] > 0.0) hit = true;
            }
            if (!hit) continue;

            // Normal component reflects and is scaled by restitution; the two
            // tangential components lose a fraction to friction.
            body.velocity[axis] = -body.velocity[axis] * bounds.restitution;
            if (bounds.friction > 0.0) {
                const double keep = 1.0 - bounds.friction;
                for (int other = 0; other < 3; ++other) {
                    if (other != axis) body.velocity[other] *= keep;
                }
            }
            ++report.boundaryBounces;
            primed_ = false;  // velocity changed outside the integrator
        }
    }
}

void GravitySystem::applyCollisions(double /*dt*/, StepReport& report) {
    if (settings_.collisionMode == CollisionMode::Ignore) return;
    const CollisionOutcome outcome = resolveCollisions(bodies_, settings_);
    report.merges += outcome.merges;
    report.elasticCollisions += outcome.elasticCollisions;
    if (outcome.merges > 0 || outcome.elasticCollisions > 0) primed_ = false;
}

void GravitySystem::recordTrails() {
    if (settings_.trailLength == 0) return;
    if (settings_.trailSampleInterval > 0.0 &&
        elapsed_ - lastTrailSample_ < settings_.trailSampleInterval) {
        return;
    }
    lastTrailSample_ = elapsed_;

    // Samples are stored already expressed in the chosen frame, so switching
    // frames does not retroactively rewrite history -- it starts a new one.
    Vec3 origin(0.0);
    if (settings_.trailReference != kInvalidBodyId) {
        if (const CelestialBody* reference = find(settings_.trailReference)) {
            origin = reference->position;
        }
    }

    for (CelestialBody& body : bodies_) {
        if (!body.showTrail) {
            if (!body.trail.empty()) body.trail.clear();
            continue;
        }
        body.trail.push_back(body.position - origin);
        while (body.trail.size() > settings_.trailLength) body.trail.pop_front();
    }
}

}  // namespace sim
