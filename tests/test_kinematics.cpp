#include "TestFramework.h"

#include <cmath>

#include "sim/Constants.h"
#include "sim/GravitySystem.h"

using namespace sim;

namespace {

// The M2 kinematics lab: uniform 9.81 m/s^2 downwards, a floor to bounce off,
// and no pairwise gravity.
GravitySystem makeBounceLab(double restitution, double friction = 0.0) {
    GravitySystem system;
    system.settings().pairwiseGravityEnabled = false;
    system.settings().uniformGravity = Vec3(0.0, -constants::kEarthSurfaceGravity, 0.0);
    system.settings().trailLength = 0;
    system.settings().bounds.enabled = true;
    system.settings().bounds.min = Vec3(-20.0, 0.0, -20.0);
    system.settings().bounds.max = Vec3(20.0, 40.0, 20.0);
    system.settings().bounds.restitution = restitution;
    system.settings().bounds.friction = friction;

    CelestialBody ball;
    ball.name = "ball";
    ball.mass = 1.0;
    ball.radius = 0.0;  // a point, so the floor is exactly y = 0
    ball.position = Vec3(0.0, 10.0, 0.0);
    ball.showTrail = false;
    system.add(ball);
    return system;
}

}  // namespace

TEST(kinematics_free_fall_matches_the_analytic_solution) {
    GravitySystem system = makeBounceLab(1.0);
    system.settings().bounds.enabled = false;  // let it fall past the floor

    const double dt = 1.0 / 120.0;
    const int steps = 120;  // 1 s
    for (int i = 0; i < steps; ++i) system.step(dt);

    const double g = constants::kEarthSurfaceGravity;
    const double t = steps * dt;
    CHECK_REL(system.bodies()[0].position.y, 10.0 - 0.5 * g * t * t, 1e-12);
    CHECK_REL(system.bodies()[0].velocity.y, -g * t, 1e-12);
}

TEST(kinematics_fall_time_to_the_floor_is_correct) {
    // Dropped from 10 m, the floor is reached at t = sqrt(2h/g) ~= 1.4278 s.
    GravitySystem system = makeBounceLab(0.0);
    const double dt = 1.0 / 2000.0;
    double t = 0.0;
    while (system.bodies()[0].position.y > 1e-6 && t < 10.0) {
        system.step(dt);
        t += dt;
    }
    const double expected = std::sqrt(2.0 * 10.0 / constants::kEarthSurfaceGravity);
    CHECK_NEAR(t, expected, 2.0 * dt);
}

TEST(kinematics_impact_speed_matches_energy_conservation) {
    // v = sqrt(2 g h) at the floor.
    GravitySystem system = makeBounceLab(1.0);
    const double dt = 1.0 / 4000.0;
    double impactSpeed = 0.0;
    for (int i = 0; i < 8000; ++i) {
        const double before = system.bodies()[0].velocity.y;
        const StepReport report = system.step(dt);
        if (report.boundaryBounces > 0) {
            impactSpeed = std::abs(before);
            break;
        }
    }
    const double expected = std::sqrt(2.0 * constants::kEarthSurfaceGravity * 10.0);
    CHECK_NEAR(impactSpeed, expected, 0.02);
}

TEST(kinematics_perfectly_elastic_bounce_returns_to_the_drop_height) {
    GravitySystem system = makeBounceLab(1.0);
    const double dt = 1.0 / 4000.0;
    double peak = 0.0;
    bool bounced = false;
    for (int i = 0; i < 40000; ++i) {
        const StepReport report = system.step(dt);
        if (report.boundaryBounces > 0) bounced = true;
        if (bounced) peak = std::max(peak, system.bodies()[0].position.y);
        if (bounced && system.bodies()[0].velocity.y < 0.0 && peak > 0.0) break;
    }
    CHECK(bounced);
    CHECK_NEAR(peak, 10.0, 0.05);
}

TEST(kinematics_restitution_scales_the_rebound_height_quadratically) {
    // Rebound height goes as e^2: e = 0.5 must return to a quarter of 10 m.
    GravitySystem system = makeBounceLab(0.5);
    const double dt = 1.0 / 4000.0;
    double peak = 0.0;
    bool bounced = false;
    for (int i = 0; i < 40000; ++i) {
        const StepReport report = system.step(dt);
        if (report.boundaryBounces > 0) bounced = true;
        if (bounced) {
            peak = std::max(peak, system.bodies()[0].position.y);
            if (system.bodies()[0].velocity.y < 0.0 && peak > 0.1) break;
        }
    }
    CHECK_NEAR(peak, 10.0 * 0.25, 0.05);
}

TEST(kinematics_inelastic_ball_settles_on_the_floor) {
    GravitySystem system = makeBounceLab(0.0);
    const double dt = 1.0 / 500.0;
    for (int i = 0; i < 5000; ++i) system.step(dt);
    CHECK_NEAR(system.bodies()[0].position.y, 0.0, 1e-6);
    CHECK(std::abs(system.bodies()[0].velocity.y) < 0.05);
}

TEST(kinematics_friction_removes_tangential_speed_on_bounce) {
    GravitySystem system = makeBounceLab(0.8, 0.5);
    system.bodies()[0].velocity = Vec3(4.0, 0.0, 0.0);
    system.invalidate();

    const double dt = 1.0 / 2000.0;
    for (int i = 0; i < 20000; ++i) {
        const StepReport report = system.step(dt);
        if (report.boundaryBounces > 0) break;
    }
    // One bounce with friction 0.5 halves the horizontal speed.
    CHECK_NEAR(system.bodies()[0].velocity.x, 2.0, 1e-9);
}

TEST(kinematics_ball_stays_inside_the_box) {
    GravitySystem system = makeBounceLab(0.9);
    system.bodies()[0].velocity = Vec3(30.0, 5.0, -22.0);
    system.invalidate();

    const double dt = 1.0 / 500.0;
    const Bounds& bounds = system.settings().bounds;
    for (int i = 0; i < 20000; ++i) {
        system.step(dt);
        const Vec3 p = system.bodies()[0].position;
        CHECK(p.x >= bounds.min.x - 1e-9 && p.x <= bounds.max.x + 1e-9);
        CHECK(p.y >= bounds.min.y - 1e-9 && p.y <= bounds.max.y + 1e-9);
        CHECK(p.z >= bounds.min.z - 1e-9 && p.z <= bounds.max.z + 1e-9);
    }
}

TEST(kinematics_energy_is_conserved_while_airborne) {
    // KE + m g h must be constant between bounces.
    GravitySystem system = makeBounceLab(1.0);
    system.settings().bounds.enabled = false;
    const double g = constants::kEarthSurfaceGravity;
    const CelestialBody& ball = system.bodies()[0];
    const double initial = 0.5 * ball.mass * lengthSquared(ball.velocity) +
                          ball.mass * g * ball.position.y;

    const double dt = 1.0 / 240.0;
    for (int i = 0; i < 200; ++i) {
        system.step(dt);
        const double energy = 0.5 * ball.mass * lengthSquared(ball.velocity) +
                              ball.mass * g * ball.position.y;
        CHECK_REL(energy, initial, 1e-10);
    }
}
