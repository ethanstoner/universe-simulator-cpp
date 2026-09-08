#pragma once

#include <vector>

#include "sim/Vec.h"

namespace sim {

enum class IntegratorType {
    ExplicitEuler,   // first order, not symplectic: orbits spiral outwards
    SymplecticEuler,  // first order, symplectic: bounded energy error
    VelocityVerlet,   // second order, symplectic, time reversible -- the default
    RungeKutta4,      // fourth order, NOT symplectic: accurate but drifts secularly
};

const char* integratorName(IntegratorType type);
int integratorCount();
IntegratorType integratorFromIndex(int index);

// Supplies a(x) for the whole system. Implemented by GravitySystem; kept
// abstract so the integrators can be unit tested against analytic fields
// (constant gravity, a harmonic oscillator) with no bodies involved.
class ForceModel {
public:
    virtual ~ForceModel() = default;

    // Writes one acceleration per body. `out` is sized by the caller.
    // Must be a pure function of `positions` (and the model's own masses):
    // velocity Verlet and RK4 both evaluate it at trial positions.
    virtual void accelerations(const std::vector<Vec3>& positions,
                               std::vector<Vec3>& out) const = 0;
};

// Advances positions/velocities by exactly `dt`.
//
// `accelerations` is in/out: on entry it must hold a(x_n) (velocity Verlet
// needs it and re-using it saves one force evaluation per step); on exit it
// holds a(x_{n+1}). Call `primeAccelerations` once before the first step.
void integrate(IntegratorType type, const ForceModel& model, double dt,
               std::vector<Vec3>& positions, std::vector<Vec3>& velocities,
               std::vector<Vec3>& accelerations);

void primeAccelerations(const ForceModel& model, const std::vector<Vec3>& positions,
                        std::vector<Vec3>& accelerations);

// True for integrators that conserve a shadow Hamiltonian, i.e. whose energy
// error stays bounded instead of growing without limit.
bool isSymplectic(IntegratorType type);

}  // namespace sim
