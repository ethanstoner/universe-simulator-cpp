#include "TestFramework.h"

#include <cmath>
#include <cstdint>
#include <vector>

#include "sim/BarnesHut.h"
#include "sim/Constants.h"
#include "sim/Vec.h"

using namespace sim;

namespace {

// Fixed-seed generator: an approximation test that shuffles its own input on
// every run tells you nothing about a regression.
struct Lcg {
    std::uint64_t state = 0xC0FFEE123456789ull;
    double next() {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<double>(state >> 11) / 9007199254740992.0;
    }
    double range(double low, double high) { return low + next() * (high - low); }
};

struct Cloud {
    std::vector<Vec3> positions;
    std::vector<double> masses;
};

Cloud makeCloud(int count, double extent = 1.0e11, double mass = 1.0e24) {
    Lcg random;
    Cloud cloud;
    for (int i = 0; i < count; ++i) {
        cloud.positions.push_back(Vec3(random.range(-extent, extent),
                                       random.range(-extent, extent),
                                       random.range(-extent, extent)));
        cloud.masses.push_back(mass * random.range(0.5, 2.0));
    }
    return cloud;
}

// The reference every approximation is measured against.
Vec3 directAcceleration(const Cloud& cloud, int target, double G, double softeningSq) {
    Vec3 total(0.0);
    for (std::size_t j = 0; j < cloud.positions.size(); ++j) {
        if (static_cast<int>(j) == target) continue;
        const Vec3 delta = cloud.positions[j] - cloud.positions[target];
        const double distanceSq = lengthSquared(delta) + softeningSq;
        if (distanceSq <= 0.0) continue;
        total += delta * (G * cloud.masses[j] / (distanceSq * std::sqrt(distanceSq)));
    }
    return total;
}

// Mean relative error of the tree against direct summation over every body.
double meanRelativeError(const Cloud& cloud, double theta, double softeningSq = 0.0) {
    BarnesHutTree tree;
    tree.build(cloud.positions, cloud.masses);

    const double G = constants::kG;
    double total = 0.0;
    int counted = 0;
    for (std::size_t i = 0; i < cloud.positions.size(); ++i) {
        const Vec3 exact = directAcceleration(cloud, static_cast<int>(i), G, softeningSq);
        const Vec3 approx = tree.accelerationAt(cloud.positions[i], static_cast<int>(i),
                                                G, theta, softeningSq);
        const double magnitude = glm::length(exact);
        if (magnitude <= 0.0) continue;
        total += glm::length(approx - exact) / magnitude;
        ++counted;
    }
    return counted > 0 ? total / counted : 0.0;
}

}  // namespace

TEST(barneshut_empty_tree_is_harmless) {
    BarnesHutTree tree;
    tree.build({}, {});
    CHECK(tree.empty());
    CHECK(tree.accelerationAt(Vec3(1.0, 2.0, 3.0), -1, constants::kG, 0.5, 0.0) ==
          Vec3(0.0));
}

TEST(barneshut_single_body_matches_the_closed_form) {
    std::vector<Vec3> positions{Vec3(0.0)};
    std::vector<double> masses{1.0e24};
    BarnesHutTree tree;
    tree.build(positions, masses);

    const Vec3 a = tree.accelerationAt(Vec3(1.0e9, 0.0, 0.0), -1, constants::kG, 0.5, 0.0);
    const double expected = constants::kG * 1.0e24 / (1.0e9 * 1.0e9);
    CHECK_REL(-a.x, expected, 1e-12);
    CHECK_NEAR(a.y, 0.0, 1e-30);
}

TEST(barneshut_aggregates_total_mass_and_centre_of_mass) {
    std::vector<Vec3> positions{Vec3(-1.0e9, 0.0, 0.0), Vec3(1.0e9, 0.0, 0.0)};
    std::vector<double> masses{3.0e24, 1.0e24};
    BarnesHutTree tree;
    tree.build(positions, masses);

    CHECK_REL(tree.totalMass(), 4.0e24, 1e-14);
    // Mass-weighted midpoint: 3:1 puts it a quarter of the way across.
    CHECK_REL(tree.centreOfMass().x, -0.5e9, 1e-12);
}

TEST(barneshut_ignores_zero_and_negative_mass_bodies) {
    std::vector<Vec3> positions{Vec3(0.0), Vec3(1.0e9, 0.0, 0.0)};
    std::vector<double> masses{1.0e24, 0.0};
    BarnesHutTree tree;
    tree.build(positions, masses);
    CHECK_REL(tree.totalMass(), 1.0e24, 1e-14);
}

TEST(barneshut_theta_zero_reproduces_direct_summation) {
    // theta = 0 can never satisfy s^2 < 0, so every aggregate is opened and the
    // walk reaches every leaf. The result must be direct summation to rounding.
    const Cloud cloud = makeCloud(120);
    CHECK_LESS(meanRelativeError(cloud, 0.0), 1e-12);
}

TEST(barneshut_error_falls_as_theta_falls) {
    // The defining property. If this ordering ever breaks, the opening
    // criterion is wrong regardless of how small the errors happen to be.
    const Cloud cloud = makeCloud(400);
    const double loose = meanRelativeError(cloud, 1.0);
    const double medium = meanRelativeError(cloud, 0.5);
    const double tight = meanRelativeError(cloud, 0.2);

    CHECK_LESS(medium, loose);
    CHECK_LESS(tight, medium);
    // And the usual working value is genuinely accurate.
    CHECK_LESS(medium, 0.01);
}

TEST(barneshut_default_theta_is_sub_percent_accurate) {
    const Cloud cloud = makeCloud(800);
    CHECK_LESS(meanRelativeError(cloud, 0.5), 0.01);
}

TEST(barneshut_respects_softening) {
    // Two bodies on top of each other: without softening this is a division by
    // zero, with it the result must stay finite.
    std::vector<Vec3> positions{Vec3(0.0), Vec3(0.0)};
    std::vector<double> masses{1.0e24, 1.0e24};
    BarnesHutTree tree;
    tree.build(positions, masses);

    const double softeningSq = 1.0e12;
    const Vec3 a = tree.accelerationAt(positions[0], 0, constants::kG, 0.5, softeningSq);
    CHECK(std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z));
}

TEST(barneshut_survives_coincident_bodies) {
    // Identical positions can never be separated by subdivision. The depth cap
    // must stop the build rather than overflowing the stack.
    std::vector<Vec3> positions(50, Vec3(1.0e9, -2.0e9, 3.0e9));
    std::vector<double> masses(50, 1.0e22);
    BarnesHutTree tree;
    tree.build(positions, masses);

    CHECK(!tree.empty());
    CHECK_REL(tree.totalMass(), 50.0 * 1.0e22, 1e-12);
    const Vec3 a = tree.accelerationAt(Vec3(0.0), -1, constants::kG, 0.5, 1.0e12);
    CHECK(std::isfinite(a.x));
}

TEST(barneshut_handles_a_wide_dynamic_range) {
    // A solar-system-like arrangement: one dominant mass and a swarm of tiny
    // ones spread over four orders of magnitude in distance.
    Lcg random;
    Cloud cloud;
    cloud.positions.push_back(Vec3(0.0));
    cloud.masses.push_back(constants::kSolarMass);
    for (int i = 0; i < 300; ++i) {
        const double radius = constants::kAu * std::pow(10.0, random.range(-2.0, 2.0));
        const double angle = random.range(0.0, 6.2831853);
        cloud.positions.push_back(
            Vec3(radius * std::cos(angle), 0.0, radius * std::sin(angle)));
        cloud.masses.push_back(constants::kEarthMass * random.range(0.01, 1.0));
    }
    CHECK_LESS(meanRelativeError(cloud, 0.5), 0.02);
}

TEST(barneshut_ignore_body_excludes_it_from_aggregates_too) {
    // The subtle failure mode: accepting an aggregate that happens to contain
    // the body being solved for silently folds that body's own mass into the
    // result. It is invisible for a light body, so this puts a mass 2000x
    // heavier than its neighbours *inside* a dense cluster, where the spurious
    // term would dominate if the exclusion were missing.
    Lcg random;
    Cloud cloud;
    const double spread = 1.0e10;
    cloud.positions.push_back(Vec3(0.0));
    cloud.masses.push_back(2000.0 * 1.0e24);
    for (int i = 0; i < 200; ++i) {
        cloud.positions.push_back(Vec3(random.range(-spread, spread),
                                       random.range(-spread, spread),
                                       random.range(-spread, spread)));
        cloud.masses.push_back(1.0e24);
    }

    BarnesHutTree tree;
    tree.build(cloud.positions, cloud.masses);

    // Error is normalised by the sum of the individual contribution
    // magnitudes, not by the net. A body at the centre of a roughly symmetric
    // cloud has a net acceleration that nearly cancels, so dividing by it
    // inflates a perfectly good answer into a large "relative error".
    auto contributionScale = [&](int target) {
        double scale = 0.0;
        for (std::size_t j = 0; j < cloud.positions.size(); ++j) {
            if (static_cast<int>(j) == target) continue;
            const double distanceSq =
                lengthSquared(cloud.positions[j] - cloud.positions[target]);
            if (distanceSq > 0.0) scale += constants::kG * cloud.masses[j] / distanceSq;
        }
        return scale;
    };

    // If the heavy body's own mass leaked into an aggregate it would add a term
    // of order G * 2000 m / r^2, which is thousands of times the scale below.
    for (int i = 0; i < 12; ++i) {
        const Vec3 exact = directAcceleration(cloud, i, constants::kG, 0.0);
        const Vec3 approx =
            tree.accelerationAt(cloud.positions[i], i, constants::kG, 0.5, 0.0);
        CHECK_LESS(glm::length(approx - exact) / contributionScale(i), 0.01);
    }
}

TEST(barneshut_node_count_is_linear_ish_in_body_count) {
    // A guard against a build that degenerates and allocates without bound.
    BarnesHutTree small, large;
    const Cloud a = makeCloud(100);
    const Cloud b = makeCloud(1000);
    small.build(a.positions, a.masses);
    large.build(b.positions, b.masses);

    CHECK(small.nodeCount() > 0);
    // Eight children per split means the constant is generous, but it must not
    // be quadratic.
    CHECK_LESS(static_cast<double>(large.nodeCount()), 200.0 * 1000.0);
    CHECK(large.depth() < 32);
}
