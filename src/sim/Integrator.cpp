#include "sim/Integrator.h"

namespace sim {
namespace {

void resize(std::vector<Vec3>& v, std::size_t n) {
    if (v.size() != n) v.assign(n, Vec3(0.0));
}

}  // namespace

const char* integratorName(IntegratorType type) {
    switch (type) {
        case IntegratorType::ExplicitEuler: return "Explicit Euler";
        case IntegratorType::SymplecticEuler: return "Symplectic Euler";
        case IntegratorType::VelocityVerlet: return "Velocity Verlet";
        case IntegratorType::RungeKutta4: return "Runge-Kutta 4";
    }
    return "?";
}

int integratorCount() { return 4; }

IntegratorType integratorFromIndex(int index) {
    switch (index) {
        case 0: return IntegratorType::ExplicitEuler;
        case 1: return IntegratorType::SymplecticEuler;
        case 3: return IntegratorType::RungeKutta4;
        default: return IntegratorType::VelocityVerlet;
    }
}

bool isSymplectic(IntegratorType type) {
    return type == IntegratorType::SymplecticEuler ||
           type == IntegratorType::VelocityVerlet;
}

void primeAccelerations(const ForceModel& model, const std::vector<Vec3>& positions,
                        std::vector<Vec3>& accelerations) {
    resize(accelerations, positions.size());
    model.accelerations(positions, accelerations);
}

void integrate(IntegratorType type, const ForceModel& model, double dt,
               std::vector<Vec3>& positions, std::vector<Vec3>& velocities,
               std::vector<Vec3>& accelerations) {
    const std::size_t n = positions.size();
    if (n == 0) return;
    resize(velocities, n);
    resize(accelerations, n);

    // Scratch buffers are function-local statics-free: reallocation per step at
    // solar-system body counts is irrelevant next to the force evaluation.
    switch (type) {
        case IntegratorType::ExplicitEuler: {
            // x_{n+1} = x_n + v_n dt   (uses the OLD velocity -- this is what
            // makes it non-symplectic and lets orbital energy grow)
            // v_{n+1} = v_n + a_n dt
            std::vector<Vec3> oldVelocities = velocities;
            for (std::size_t i = 0; i < n; ++i) {
                positions[i] += oldVelocities[i] * dt;
                velocities[i] += accelerations[i] * dt;
            }
            model.accelerations(positions, accelerations);
            break;
        }

        case IntegratorType::SymplecticEuler: {
            // v_{n+1} = v_n + a_n dt
            // x_{n+1} = x_n + v_{n+1} dt   (the NEW velocity: symplectic)
            for (std::size_t i = 0; i < n; ++i) {
                velocities[i] += accelerations[i] * dt;
                positions[i] += velocities[i] * dt;
            }
            model.accelerations(positions, accelerations);
            break;
        }

        case IntegratorType::VelocityVerlet: {
            // x_{n+1} = x_n + v_n dt + 1/2 a_n dt^2
            // a_{n+1} = a(x_{n+1})
            // v_{n+1} = v_n + 1/2 (a_n + a_{n+1}) dt
            const double halfDtSq = 0.5 * dt * dt;
            for (std::size_t i = 0; i < n; ++i) {
                positions[i] += velocities[i] * dt + accelerations[i] * halfDtSq;
            }
            std::vector<Vec3> nextAcceleration(n);
            model.accelerations(positions, nextAcceleration);
            const double halfDt = 0.5 * dt;
            for (std::size_t i = 0; i < n; ++i) {
                velocities[i] += (accelerations[i] + nextAcceleration[i]) * halfDt;
            }
            accelerations.swap(nextAcceleration);
            break;
        }

        case IntegratorType::RungeKutta4: {
            // Classic RK4 on the coupled first-order system
            //   dx/dt = v,  dv/dt = a(x).
            // k_i(x) are velocities, k_i(v) are accelerations.
            const std::vector<Vec3> x0 = positions;
            const std::vector<Vec3> v0 = velocities;

            std::vector<Vec3> k1x = v0, k1v(n);
            model.accelerations(x0, k1v);

            std::vector<Vec3> trial(n);
            auto advance = [&](const std::vector<Vec3>& dx, double factor) {
                for (std::size_t i = 0; i < n; ++i) trial[i] = x0[i] + dx[i] * factor;
            };

            advance(k1x, dt * 0.5);
            std::vector<Vec3> k2x(n), k2v(n);
            for (std::size_t i = 0; i < n; ++i) k2x[i] = v0[i] + k1v[i] * (dt * 0.5);
            model.accelerations(trial, k2v);

            advance(k2x, dt * 0.5);
            std::vector<Vec3> k3x(n), k3v(n);
            for (std::size_t i = 0; i < n; ++i) k3x[i] = v0[i] + k2v[i] * (dt * 0.5);
            model.accelerations(trial, k3v);

            advance(k3x, dt);
            std::vector<Vec3> k4x(n), k4v(n);
            for (std::size_t i = 0; i < n; ++i) k4x[i] = v0[i] + k3v[i] * dt;
            model.accelerations(trial, k4v);

            const double sixth = dt / 6.0;
            for (std::size_t i = 0; i < n; ++i) {
                positions[i] = x0[i] + sixth * (k1x[i] + 2.0 * k2x[i] + 2.0 * k3x[i] + k4x[i]);
                velocities[i] = v0[i] + sixth * (k1v[i] + 2.0 * k2v[i] + 2.0 * k3v[i] + k4v[i]);
            }
            model.accelerations(positions, accelerations);
            break;
        }
    }
}

}  // namespace sim
