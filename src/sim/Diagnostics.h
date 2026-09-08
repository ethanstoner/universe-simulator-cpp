#pragma once

#include "sim/Vec.h"

namespace sim {

class GravitySystem;

// A snapshot of the conserved quantities. For an isolated N-body system with no
// collisions, energy / momentum / angular momentum are all exactly conserved by
// the true dynamics, so watching them drift is the honest way to tell whether a
// "stable" orbit is really stable or just slowly falling apart.
struct SystemDiagnostics {
    double kineticEnergy = 0.0;    // J
    double potentialEnergy = 0.0;  // J, negative
    double totalEnergy = 0.0;      // J
    Vec3 linearMomentum{0.0};      // kg m/s
    Vec3 angularMomentum{0.0};     // kg m^2/s, about the origin
    Vec3 centreOfMass{0.0};        // m
    Vec3 centreOfMassVelocity{0.0};  // m/s
    double totalMass = 0.0;        // kg
};

SystemDiagnostics computeDiagnostics(const GravitySystem& system);

// Tracks energy against a reference captured when the scene was loaded.
class EnergyTracker {
public:
    void reset(const SystemDiagnostics& reference);
    void update(const SystemDiagnostics& current);

    bool hasReference() const { return hasReference_; }
    double referenceEnergy() const { return reference_; }
    double currentEnergy() const { return current_; }

    // |E - E0| / |E0|. Reported rather than an absolute figure because the
    // absolute numbers are ~1e33 J for a solar system.
    double relativeDrift() const;
    double peakRelativeDrift() const { return peakDrift_; }

private:
    bool hasReference_ = false;
    double reference_ = 0.0;
    double current_ = 0.0;
    double peakDrift_ = 0.0;
};

}  // namespace sim
