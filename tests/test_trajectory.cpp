#include "TestFramework.h"

#include <cmath>

#include "sim/Constants.h"
#include "sim/GravitySystem.h"
#include "sim/OrbitMath.h"
#include "sim/SceneLibrary.h"
#include "sim/Trajectory.h"

using namespace sim;

namespace {

BodyId idOf(const GravitySystem& system, const std::string& name) {
    for (const CelestialBody& body : system.bodies()) {
        if (body.name == name) return body.id;
    }
    return kInvalidBodyId;
}

TrajectoryRequest requestFor(const GravitySystem& system, BodyId body,
                             double horizon, double step) {
    TrajectoryRequest request;
    request.body = body;
    request.horizonSeconds = horizon;
    request.stepSeconds = step;
    request.maxSamples = 400;
    request.stepsPerSample = 8;
    (void)system;
    return request;
}

}  // namespace

TEST(trajectory_rejects_nonsense_requests) {
    GravitySystem system;
    applyScene(*findScene("two-body"), system);
    const BodyId earth = idOf(system, "Earth");

    TrajectoryRequest bad;
    CHECK(predictTrajectory(system, bad).points.empty());  // no body

    bad.body = earth;
    CHECK(predictTrajectory(system, bad).points.empty());  // no horizon

    bad.horizonSeconds = 1000.0;
    CHECK(predictTrajectory(system, bad).points.empty());  // no step size

    bad.stepSeconds = 3600.0;
    CHECK(!predictTrajectory(system, bad).points.empty());  // now valid

    TrajectoryRequest missing = bad;
    missing.body = 999999;
    CHECK(predictTrajectory(system, missing).points.empty());
}

TEST(trajectory_does_not_disturb_the_live_system) {
    // The forecast integrates a copy. If it touched the original, the whole
    // feature would silently corrupt the simulation it is describing.
    GravitySystem system;
    applyScene(*findScene("inner"), system);
    for (int i = 0; i < 100; ++i) system.step(1800.0);

    std::vector<Vec3> before;
    for (const CelestialBody& body : system.bodies()) before.push_back(body.position);
    const double elapsedBefore = system.elapsedSimulatedSeconds();
    const std::size_t countBefore = system.size();

    predictTrajectory(system, requestFor(system, idOf(system, "Earth"),
                                         constants::kJulianYear, 1800.0));

    CHECK(system.size() == countBefore);
    CHECK(system.elapsedSimulatedSeconds() == elapsedBefore);
    for (std::size_t i = 0; i < before.size(); ++i) {
        CHECK(system.bodies()[i].position == before[i]);
    }
}

TEST(trajectory_predicts_where_the_body_actually_goes) {
    // The forecast is only worth drawing if it matches what the simulation
    // then does. Predict a quarter of Earth's year, advance the real system by
    // the same amount, and compare the endpoint.
    GravitySystem system;
    applyScene(*findScene("two-body"), system);
    const BodyId earth = idOf(system, "Earth");

    const double horizon = constants::kJulianYear * 0.25;
    const double step = 1800.0;
    const Trajectory predicted = predictTrajectory(system, requestFor(system, earth,
                                                                     horizon, step));
    CHECK(predicted.points.size() > 10);

    const long long steps = static_cast<long long>(predicted.horizonSeconds / step);
    for (long long i = 0; i < steps; ++i) system.step(step);

    const CelestialBody* body = system.find(earth);
    const Vec3 actual = body->position;
    const Vec3 forecast = predicted.points.back();
    // Same integrator, same step, same bodies: this should agree to a tiny
    // fraction of the orbital radius, not merely be close.
    CHECK_LESS(glm::length(actual - forecast) / constants::kAu, 1e-6);
}

TEST(trajectory_closes_the_loop_over_one_orbital_period) {
    // A full period of a near-circular orbit should return to its start.
    GravitySystem system;
    applyScene(*findScene("two-body"), system);
    const BodyId earth = idOf(system, "Earth");

    // One Julian year at a one-hour step is 8766 steps. At 8 steps per sample
    // that is 1096 samples, comfortably inside the cap -- at 4 it would be
    // 2192 and the path would truncate before closing.
    TrajectoryRequest request = requestFor(system, earth, constants::kJulianYear, 3600.0);
    request.maxSamples = 2000;
    request.stepsPerSample = 8;

    const Trajectory path = predictTrajectory(system, request);
    CHECK(!path.truncated);
    CHECK(path.points.size() > 100);

    const double closure = glm::length(path.points.back() - path.points.front());
    CHECK_LESS(closure / constants::kAu, 0.01);
}

TEST(trajectory_includes_perturbation_not_just_a_kepler_ellipse) {
    // The point of integrating rather than drawing a conic: the forecast must
    // respond to the rest of the system. Predicting Mercury with and without
    // Jupiter present has to give different answers.
    auto predictMercury = [](bool withJupiter) {
        GravitySystem system;
        applyScene(*findScene("solar-system"), system);
        if (!withJupiter) {
            for (const CelestialBody& body : system.bodies()) {
                if (body.name == "Jupiter") {
                    system.remove(body.id);
                    break;
                }
            }
        }
        GravitySystem& s = system;
        TrajectoryRequest request;
        request.body = idOf(s, "Mercury");
        request.horizonSeconds = 40.0 * constants::kJulianYear;
        request.stepSeconds = 7200.0;
        request.maxSamples = 4000;
        request.stepsPerSample = 20;
        return predictTrajectory(s, request);
    };

    const Trajectory with = predictMercury(true);
    const Trajectory without = predictMercury(false);
    CHECK(with.points.size() > 50);
    CHECK(with.points.size() == without.points.size());

    const double drift = glm::length(with.points.back() - without.points.back());
    // Over forty years Jupiter's perturbation moves Mercury measurably.
    CHECK(drift > 1.0e6);
}

TEST(trajectory_reference_frame_shows_the_moon_looping_the_earth) {
    // In the inertial frame the Moon's path is a gentle scallop around the Sun
    // and its orbit is invisible. Relative to the Earth it is a closed loop.
    GravitySystem system;
    applyScene(*findScene("earth-moon"), system);
    const BodyId moon = idOf(system, "Moon");
    const BodyId earth = idOf(system, "Earth");

    TrajectoryRequest request;
    request.body = moon;
    request.horizonSeconds = 27.3 * constants::kDay;  // one lunar period
    request.stepSeconds = 600.0;
    request.maxSamples = 2000;
    request.stepsPerSample = 4;

    const Trajectory inertial = predictTrajectory(system, request);
    request.referenceBody = earth;
    const Trajectory relative = predictTrajectory(system, request);

    // Inertial: the Moon travels roughly Earth's orbital distance in a month,
    // far further than the lunar orbit is wide.
    const double inertialSpan =
        glm::length(inertial.points.back() - inertial.points.front());
    CHECK(inertialSpan > 1.0e10);

    // Relative: every sample sits near the lunar distance and the path closes.
    for (const Vec3& point : relative.points) {
        const double radius = glm::length(point);
        CHECK(radius > 3.0e8);
        CHECK(radius < 4.6e8);
    }
    const double closure =
        glm::length(relative.points.back() - relative.points.front());
    CHECK_LESS(closure / 3.844e8, 0.15);
}

TEST(trajectory_respects_the_sample_cap) {
    GravitySystem system;
    applyScene(*findScene("two-body"), system);

    TrajectoryRequest request = requestFor(system, idOf(system, "Earth"),
                                           100.0 * constants::kJulianYear, 3600.0);
    request.maxSamples = 50;
    request.stepsPerSample = 1;

    const Trajectory path = predictTrajectory(system, request);
    CHECK(path.points.size() <= 50);
    CHECK(path.truncated);  // the horizon was not reached
}

TEST(trajectory_works_with_the_barnes_hut_solver) {
    // The forecast copies the system, including its octree, which caches raw
    // pointers into the original's arrays. If the copy kept those pointers this
    // would read freed or foreign memory.
    GravitySystem system;
    applyScene(*findScene("belt"), system);
    CHECK(system.settings().solver == GravitySolver::BarnesHut);

    TrajectoryRequest request = requestFor(system, idOf(system, "Jupiter"),
                                           2.0 * constants::kJulianYear, 7200.0);
    const Trajectory path = predictTrajectory(system, request);

    CHECK(path.points.size() > 10);
    for (const Vec3& point : path.points) {
        CHECK(std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z));
    }
}

TEST(trajectory_in_its_own_frame_is_degenerate) {
    // Documents the edge case the application guards against: expressing a
    // body's path relative to itself is identically zero, so every sample sits
    // on the origin and the drawn path collapses to a point.
    GravitySystem system;
    applyScene(*findScene("earth-moon"), system);
    const BodyId earth = idOf(system, "Earth");

    TrajectoryRequest request = requestFor(system, earth, 10.0 * constants::kDay, 600.0);
    request.referenceBody = earth;

    const Trajectory path = predictTrajectory(system, request);
    CHECK(path.points.size() > 2);
    for (const Vec3& point : path.points) {
        CHECK(glm::length(point) == 0.0);
    }
}
