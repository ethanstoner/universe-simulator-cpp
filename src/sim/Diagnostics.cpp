#include "sim/Diagnostics.h"

#include <cmath>

#include "sim/GravitySystem.h"

namespace sim {

SystemDiagnostics computeDiagnostics(const GravitySystem& system) {
    SystemDiagnostics out;
    const std::vector<CelestialBody>& bodies = system.bodies();
    const SimulationSettings& settings = system.settings();
    const double G = settings.gravitationalConstant;

    for (const CelestialBody& body : bodies) {
        out.kineticEnergy += 0.5 * body.mass * lengthSquared(body.velocity);
        out.linearMomentum += body.momentum();
        out.angularMomentum += glm::cross(body.position, body.momentum());
        out.centreOfMass += body.position * body.mass;
        out.centreOfMassVelocity += body.momentum();
        out.totalMass += body.mass;

        // Potential energy of the uniform field, if one is in use (kinematics
        // lab). U = -m g . x, so for g = (0,-9.81,0) this is +m*9.81*y.
        out.potentialEnergy -= body.mass * glm::dot(settings.uniformGravity, body.position);
    }

    if (out.totalMass > 0.0) {
        out.centreOfMass /= out.totalMass;
        out.centreOfMassVelocity /= out.totalMass;
    }

    if (settings.pairwiseGravityEnabled) {
        // U = -G m1 m2 / r, summed once per unordered pair. The same
        // stabilisation the force uses is applied here, otherwise the reported
        // energy would not be the energy the integrator is actually conserving.
        const double softeningSq = settings.softeningLength * settings.softeningLength;
        const double minDistanceSq = settings.minimumDistance * settings.minimumDistance;

        for (std::size_t i = 0; i < bodies.size(); ++i) {
            for (std::size_t j = i + 1; j < bodies.size(); ++j) {
                double distanceSq = lengthSquared(bodies[j].position - bodies[i].position);
                switch (settings.stabilization) {
                    case Stabilization::None: break;
                    case Stabilization::Softening: distanceSq += softeningSq; break;
                    case Stabilization::MinDistance:
                        distanceSq = std::max(distanceSq, minDistanceSq);
                        break;
                }
                if (distanceSq <= 0.0) continue;
                out.potentialEnergy -=
                    G * bodies[i].mass * bodies[j].mass / std::sqrt(distanceSq);
            }
        }
    }

    out.totalEnergy = out.kineticEnergy + out.potentialEnergy;
    return out;
}

void EnergyTracker::reset(const SystemDiagnostics& reference) {
    reference_ = reference.totalEnergy;
    current_ = reference.totalEnergy;
    peakDrift_ = 0.0;
    hasReference_ = true;
}

void EnergyTracker::update(const SystemDiagnostics& current) {
    current_ = current.totalEnergy;
    if (!hasReference_) return;
    const double drift = relativeDrift();
    if (drift > peakDrift_) peakDrift_ = drift;
}

double EnergyTracker::relativeDrift() const {
    if (!hasReference_ || reference_ == 0.0) return 0.0;
    return std::abs((current_ - reference_) / reference_);
}

}  // namespace sim
