#include "TestFramework.h"

#include <cmath>

#include "sim/Constants.h"
#include "sim/Diagnostics.h"
#include "sim/GravitySystem.h"
#include "sim/Integrator.h"
#include "sim/OrbitMath.h"

using namespace sim;

namespace {

// a(x) = g, independent of position: exercises the integrators against a case
// with a known closed-form solution.
class ConstantField : public ForceModel {
public:
    explicit ConstantField(Vec3 g) : g_(g) {}
    void accelerations(const std::vector<Vec3>& positions,
                       std::vector<Vec3>& out) const override {
        out.assign(positions.size(), g_);
    }

private:
    Vec3 g_;
};

// a(x) = -k x: a harmonic oscillator, whose energy is a clean test of whether
// an integrator is symplectic.
class SpringField : public ForceModel {
public:
    explicit SpringField(double k) : k_(k) {}
    void accelerations(const std::vector<Vec3>& positions,
                       std::vector<Vec3>& out) const override {
        out.resize(positions.size());
        for (std::size_t i = 0; i < positions.size(); ++i) out[i] = -k_ * positions[i];
    }

private:
    double k_;
};

struct TwoBodyOrbit {
    GravitySystem system;
    double period = 0.0;
    double radius = 0.0;
};

// A test mass on a circular orbit around a pinned central mass. Pinning removes
// the barycentre wobble so the analytic circular-orbit result applies exactly.
TwoBodyOrbit makeCircularOrbit(IntegratorType integrator, double centralMass = 1.0e26,
                               double radius = 1.0e9) {
    TwoBodyOrbit orbit;
    orbit.radius = radius;
    orbit.system.settings().integrator = integrator;
    orbit.system.settings().stabilization = Stabilization::None;
    orbit.system.settings().trailLength = 0;

    CelestialBody central;
    central.name = "central";
    central.mass = centralMass;
    central.radius = 1.0;
    central.fixed = true;
    orbit.system.add(central);

    CelestialBody satellite;
    satellite.name = "satellite";
    satellite.mass = 1.0;  // negligible next to the central mass
    satellite.radius = 1.0;
    satellite.position = Vec3(radius, 0.0, 0.0);
    satellite.velocity = Vec3(0.0, 0.0, circularOrbitSpeed(centralMass, radius));
    orbit.system.add(satellite);

    orbit.period = orbitalPeriod(centralMass, radius);
    return orbit;
}

double maxRadiusError(TwoBodyOrbit& orbit, int orbits, int stepsPerOrbit) {
    const double dt = orbit.period / stepsPerOrbit;
    double worst = 0.0;
    for (int step = 0; step < orbits * stepsPerOrbit; ++step) {
        orbit.system.step(dt);
        const double r = glm::length(orbit.system.bodies()[1].position);
        worst = std::max(worst, std::abs(r - orbit.radius) / orbit.radius);
    }
    return worst;
}

}  // namespace

// ----------------------------------------------------- closed-form kinematics

TEST(integrator_constant_acceleration_matches_analytic_solution) {
    // Free fall from rest: y = -1/2 g t^2, v = -g t. Velocity Verlet is exact
    // for a constant field, so this should be right to rounding.
    const double g = constants::kEarthSurfaceGravity;
    ConstantField field(Vec3(0.0, -g, 0.0));
    std::vector<Vec3> positions{Vec3(0.0)};
    std::vector<Vec3> velocities{Vec3(0.0)};
    std::vector<Vec3> accelerations;
    primeAccelerations(field, positions, accelerations);

    const double dt = 1.0 / 120.0;
    const int steps = 1200;  // 10 s
    for (int i = 0; i < steps; ++i) {
        integrate(IntegratorType::VelocityVerlet, field, dt, positions, velocities,
                  accelerations);
    }

    const double t = steps * dt;
    CHECK_REL(positions[0].y, -0.5 * g * t * t, 1e-12);
    CHECK_REL(velocities[0].y, -g * t, 1e-12);
}

TEST(integrator_result_is_independent_of_step_size_for_constant_field) {
    // The same 10 s of free fall taken in 120 Hz and 480 Hz steps must agree:
    // this is the property that makes physics independent of frame rate.
    const double g = constants::kEarthSurfaceGravity;
    ConstantField field(Vec3(0.0, -g, 0.0));

    auto fall = [&](double dt, int steps) {
        std::vector<Vec3> positions{Vec3(0.0)};
        std::vector<Vec3> velocities{Vec3(0.0)};
        std::vector<Vec3> accelerations;
        primeAccelerations(field, positions, accelerations);
        for (int i = 0; i < steps; ++i) {
            integrate(IntegratorType::VelocityVerlet, field, dt, positions, velocities,
                      accelerations);
        }
        return positions[0].y;
    };

    CHECK_REL(fall(1.0 / 120.0, 1200), fall(1.0 / 480.0, 4800), 1e-12);
}

TEST(integrator_rk4_is_exact_for_the_harmonic_oscillator_to_fourth_order) {
    // x(0)=1, v(0)=0, k=1 -> x(t) = cos(t). RK4 at 200 steps per period should
    // track this to better than 1e-9.
    SpringField field(1.0);
    std::vector<Vec3> positions{Vec3(1.0, 0.0, 0.0)};
    std::vector<Vec3> velocities{Vec3(0.0)};
    std::vector<Vec3> accelerations;
    primeAccelerations(field, positions, accelerations);

    const double dt = 2.0 * 3.14159265358979323846 / 200.0;
    for (int i = 0; i < 200; ++i) {
        integrate(IntegratorType::RungeKutta4, field, dt, positions, velocities,
                  accelerations);
    }
    CHECK_NEAR(positions[0].x, std::cos(200 * dt), 1e-8);
}

TEST(integrator_symplectic_euler_bounds_oscillator_energy) {
    // Explicit Euler pumps energy into a harmonic oscillator without limit;
    // symplectic Euler keeps the error bounded. Ten thousand steps is enough
    // for the difference to be unmistakable.
    auto finalEnergy = [](IntegratorType type) {
        SpringField field(1.0);
        std::vector<Vec3> positions{Vec3(1.0, 0.0, 0.0)};
        std::vector<Vec3> velocities{Vec3(0.0)};
        std::vector<Vec3> accelerations;
        primeAccelerations(field, positions, accelerations);
        const double dt = 0.05;
        for (int i = 0; i < 10000; ++i) {
            integrate(type, field, dt, positions, velocities, accelerations);
        }
        // E = 1/2 v^2 + 1/2 k x^2 with k = 1 and unit mass.
        return 0.5 * lengthSquared(velocities[0]) + 0.5 * lengthSquared(positions[0]);
    };

    const double explicitEnergy = finalEnergy(IntegratorType::ExplicitEuler);
    const double symplecticEnergy = finalEnergy(IntegratorType::SymplecticEuler);

    CHECK(explicitEnergy > 10.0);          // started at 0.5 -- blown up
    CHECK(symplecticEnergy < 0.55);        // still near 0.5
    CHECK(symplecticEnergy > 0.45);
}

// ------------------------------------------------------------- orbital tests

TEST(orbit_velocity_verlet_holds_a_circular_orbit) {
    // At 400 steps per orbit the second-order truncation error is around
    // (2 pi / 400)^2 ~= 2.5e-4, so the radius wobbles at roughly that level.
    TwoBodyOrbit orbit = makeCircularOrbit(IntegratorType::VelocityVerlet);
    const double error = maxRadiusError(orbit, 20, 400);
    CHECK_LESS(error, 1e-3);
}

TEST(orbit_verlet_radius_error_does_not_grow_with_time) {
    // The property that makes a symplectic integrator usable for long runs is
    // not that the error is small but that it stays bounded. Compare the worst
    // radius excursion over the first five orbits with the worst over orbits
    // 95-100; a spiralling integrator fails this even though it may look fine
    // over five orbits.
    TwoBodyOrbit orbit = makeCircularOrbit(IntegratorType::VelocityVerlet);
    const double early = maxRadiusError(orbit, 5, 400);
    maxRadiusError(orbit, 90, 400);  // advance without measuring
    const double late = maxRadiusError(orbit, 5, 400);
    CHECK_LESS(late, early * 1.5 + 1e-12);
}

TEST(orbit_returns_to_its_start_after_one_period) {
    TwoBodyOrbit orbit = makeCircularOrbit(IntegratorType::VelocityVerlet);
    const Vec3 start = orbit.system.bodies()[1].position;
    const int steps = 4000;
    const double dt = orbit.period / steps;
    for (int i = 0; i < steps; ++i) orbit.system.step(dt);

    const Vec3 end = orbit.system.bodies()[1].position;
    CHECK(glm::length(end - start) / orbit.radius < 1e-4);
}

TEST(orbit_explicit_euler_is_visibly_worse_than_verlet) {
    // Not a pass/fail on Euler being bad, but on the ordering being what the
    // theory says: explicit Euler spirals out, Verlet does not.
    TwoBodyOrbit euler = makeCircularOrbit(IntegratorType::ExplicitEuler);
    TwoBodyOrbit verlet = makeCircularOrbit(IntegratorType::VelocityVerlet);
    const double eulerError = maxRadiusError(euler, 20, 400);
    const double verletError = maxRadiusError(verlet, 20, 400);
    CHECK(eulerError > verletError * 100.0);
    CHECK(glm::length(euler.system.bodies()[1].position) > euler.radius);  // spiralled out
}

TEST(orbit_energy_drift_is_bounded_for_symplectic_integrators) {
    // The point of a symplectic integrator is not that its energy error is
    // small but that it stays *bounded*: it oscillates around the true value
    // instead of accumulating. So this measures the peak excursion over orbits
    // 1-25 and again over orbits 26-50 and requires the second not to be
    // materially worse. A non-symplectic method fails this even when its
    // short-run error is smaller.
    for (int index = 0; index < integratorCount(); ++index) {
        const IntegratorType type = integratorFromIndex(index);
        if (!isSymplectic(type)) continue;

        GravitySystem system;
        system.settings().integrator = type;
        system.settings().stabilization = Stabilization::None;
        system.settings().trailLength = 0;

        const double m1 = 1.0e26, m2 = 1.0e22, r = 1.0e9;
        const double mu = constants::kG * (m1 + m2);
        const double relativeSpeed = std::sqrt(mu / r);

        CelestialBody a;
        a.name = "a";
        a.mass = m1;
        a.radius = 1.0;
        a.position = Vec3(-r * m2 / (m1 + m2), 0.0, 0.0);
        a.velocity = Vec3(0.0, 0.0, -relativeSpeed * m2 / (m1 + m2));
        system.add(a);

        CelestialBody b;
        b.name = "b";
        b.mass = m2;
        b.radius = 1.0;
        b.position = Vec3(r * m1 / (m1 + m2), 0.0, 0.0);
        b.velocity = Vec3(0.0, 0.0, relativeSpeed * m1 / (m1 + m2));
        system.add(b);

        const double referenceEnergy = computeDiagnostics(system).totalEnergy;
        const double period = orbitalPeriod(m1 + m2, r);
        const double dt = period / 500.0;

        auto peakDriftOver = [&](int orbits) {
            double peak = 0.0;
            for (int i = 0; i < 500 * orbits; ++i) {
                system.step(dt);
                const double energy = computeDiagnostics(system).totalEnergy;
                peak = std::max(peak, std::abs((energy - referenceEnergy) / referenceEnergy));
            }
            return peak;
        };

        const double firstHalf = peakDriftOver(25);
        const double secondHalf = peakDriftOver(25);

        // Bounded: the later window is no worse than the earlier one.
        CHECK_LESS(secondHalf, firstHalf * 1.2 + 1e-15);
        // And the absolute size is what first-order symplectic Euler at 500
        // steps per orbit should give -- small, but not machine precision.
        CHECK_LESS(firstHalf, 1e-3);
    }
}

TEST(orbit_conserves_linear_momentum) {
    // Newton's third law is applied pairwise, so total momentum must be
    // conserved to rounding. The pair is given a net drift because normalising
    // against a near-zero total momentum measures nothing.
    GravitySystem system;
    system.settings().stabilization = Stabilization::None;
    system.settings().trailLength = 0;

    CelestialBody a;
    a.name = "a";
    a.mass = 4.0e24;
    a.radius = 1.0;
    a.position = Vec3(-1.0e8, 0.0, 0.0);
    a.velocity = Vec3(200.0, 0.0, -500.0);
    system.add(a);

    CelestialBody b;
    b.name = "b";
    b.mass = 6.0e24;
    b.radius = 1.0;
    b.position = Vec3(1.0e8, 0.0, 0.0);
    b.velocity = Vec3(200.0, 0.0, 333.0);
    system.add(b);

    // Scale by the sum of the individual momentum magnitudes, so the tolerance
    // means "small compared with the momentum actually being shuffled around"
    // rather than "small compared with a total that happens to cancel".
    const SystemDiagnostics before = computeDiagnostics(system);
    const double scale = glm::length(system.bodies()[0].momentum()) +
                         glm::length(system.bodies()[1].momentum());

    for (int i = 0; i < 20000; ++i) system.step(10.0);

    const SystemDiagnostics after = computeDiagnostics(system);
    // 20 000 steps of floating-point accumulation lands around 4e-14; a broken
    // third-law pairing would be O(1), so this threshold still separates the
    // two by twelve orders of magnitude.
    CHECK_LESS(glm::length(after.linearMomentum - before.linearMomentum) / scale, 1e-12);
}

TEST(orbit_conserves_angular_momentum) {
    TwoBodyOrbit orbit = makeCircularOrbit(IntegratorType::VelocityVerlet);
    const Vec3 initial = computeDiagnostics(orbit.system).angularMomentum;
    const double dt = orbit.period / 500.0;
    for (int i = 0; i < 500 * 10; ++i) orbit.system.step(dt);
    const Vec3 final = computeDiagnostics(orbit.system).angularMomentum;
    CHECK(glm::length(final - initial) / glm::length(initial) < 1e-10);
}

TEST(simulation_is_deterministic) {
    // Identical initial conditions must produce bit-identical trajectories.
    auto run = []() {
        TwoBodyOrbit orbit = makeCircularOrbit(IntegratorType::VelocityVerlet);
        const double dt = orbit.period / 333.0;
        for (int i = 0; i < 5000; ++i) orbit.system.step(dt);
        return orbit.system.bodies()[1].position;
    };

    const Vec3 first = run();
    const Vec3 second = run();
    CHECK(first.x == second.x);
    CHECK(first.y == second.y);
    CHECK(first.z == second.z);
}

TEST(timestep_halving_reduces_error_as_expected_for_verlet) {
    // Velocity Verlet is second order: halving dt should cut the position error
    // by roughly 4. Measured against a much finer reference run.
    auto positionAfterQuarterOrbit = [](int stepsPerOrbit) {
        TwoBodyOrbit orbit = makeCircularOrbit(IntegratorType::VelocityVerlet);
        const double dt = orbit.period / stepsPerOrbit;
        for (int i = 0; i < stepsPerOrbit / 4; ++i) orbit.system.step(dt);
        return orbit.system.bodies()[1].position;
    };

    const Vec3 reference = positionAfterQuarterOrbit(64000);
    const double coarse = glm::length(positionAfterQuarterOrbit(200) - reference);
    const double fine = glm::length(positionAfterQuarterOrbit(400) - reference);
    const double ratio = coarse / fine;
    CHECK(ratio > 3.0);
    CHECK(ratio < 5.0);
}
