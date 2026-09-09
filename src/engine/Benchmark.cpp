#include "engine/Benchmark.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "sim/BarnesHut.h"
#include "sim/Constants.h"
#include "sim/GravitySystem.h"

namespace engine {
namespace {

// Fixed seed: a benchmark whose input changes between runs cannot be compared
// against a previous run.
struct Lcg {
    std::uint64_t state = 0x5EED1234ABCDull;
    double next() {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<double>(state >> 11) / 9007199254740992.0;
    }
    double range(double low, double high) { return low + next() * (high - low); }
};

// A disc rather than a cube: closer to the geometry the simulator actually
// runs, and it stresses the tree differently because the mass is not uniform.
void makeDisc(int count, std::vector<sim::Vec3>& positions,
              std::vector<double>& masses) {
    Lcg random;
    positions.clear();
    masses.clear();
    positions.reserve(count);
    masses.reserve(count);
    for (int i = 0; i < count; ++i) {
        const double radius = sim::constants::kAu * std::sqrt(random.next()) * 40.0;
        const double angle = random.range(0.0, 6.283185307179586);
        positions.push_back(sim::Vec3(radius * std::cos(angle),
                                      random.range(-0.02, 0.02) * radius,
                                      radius * std::sin(angle)));
        masses.push_back(sim::constants::kEarthMass * random.range(0.1, 10.0));
    }
}

double seconds(const std::chrono::steady_clock::time_point& start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
        .count();
}

}  // namespace

int runBenchmark(int maxBodies) {
    if (maxBodies < 64) maxBodies = 64;

    std::printf("\nN-body force evaluation: direct summation vs Barnes-Hut\n");
    std::printf("Plummer softening 1e8 m, theta = 0.5, one full force "
                "evaluation per row.\n");
    std::printf("Accuracy is the mean relative error against direct summation.\n\n");
    std::printf("%8s  %12s  %12s  %8s  %12s\n", "bodies", "direct (ms)",
                "Barnes-Hut", "speedup", "mean error");
    std::printf("%8s  %12s  %12s  %8s  %12s\n", "------", "-----------",
                "----------", "-------", "----------");

    const double G = sim::constants::kG;
    const double softeningSq = 1.0e8 * 1.0e8;
    const double theta = 0.5;

    int crossover = 0;
    double bestSpeedup = 0.0;
    int bestSpeedupCount = 0;
    double previousDirect = 0.0;
    double previousTree = 0.0;
    double directGrowth = 0.0;
    double treeGrowth = 0.0;

    for (int count = 128; count <= maxBodies; count *= 2) {
        std::vector<sim::Vec3> positions;
        std::vector<double> masses;
        makeDisc(count, positions, masses);

        // --- direct summation, the same pairwise loop GravitySystem uses -----
        std::vector<sim::Vec3> exact(count, sim::Vec3(0.0));
        const auto directStart = std::chrono::steady_clock::now();
        for (int i = 0; i < count; ++i) {
            for (int j = i + 1; j < count; ++j) {
                const sim::Vec3 delta = positions[j] - positions[i];
                const double distanceSq = sim::lengthSquared(delta) + softeningSq;
                const double inverse = 1.0 / (distanceSq * std::sqrt(distanceSq));
                exact[i] += delta * (G * masses[j] * inverse);
                exact[j] -= delta * (G * masses[i] * inverse);
            }
        }
        const double directSeconds = seconds(directStart);

        // --- Barnes-Hut, build plus walk ------------------------------------
        std::vector<sim::Vec3> approx(count, sim::Vec3(0.0));
        const auto treeStart = std::chrono::steady_clock::now();
        sim::BarnesHutTree tree;
        tree.build(positions, masses);
        for (int i = 0; i < count; ++i) {
            approx[i] = tree.accelerationAt(positions[i], i, G, theta, softeningSq);
        }
        const double treeSeconds = seconds(treeStart);

        double error = 0.0;
        int counted = 0;
        for (int i = 0; i < count; ++i) {
            const double magnitude = glm::length(exact[i]);
            if (magnitude <= 0.0) continue;
            error += glm::length(approx[i] - exact[i]) / magnitude;
            ++counted;
        }
        if (counted > 0) error /= counted;

        const double speedup = treeSeconds > 0.0 ? directSeconds / treeSeconds : 0.0;
        std::printf("%8d  %12.3f  %12.3f  %7.1fx  %12.2e\n", count,
                    directSeconds * 1000.0, treeSeconds * 1000.0, speedup, error);

        if (crossover == 0 && speedup > 1.0) crossover = count;
        if (speedup > bestSpeedup) {
            bestSpeedup = speedup;
            bestSpeedupCount = count;
        }
        // Growth factor per doubling of N: 4 is quadratic, ~2 is N log N.
        if (previousDirect > 0.0) directGrowth = directSeconds / previousDirect;
        if (previousTree > 0.0) treeGrowth = treeSeconds / previousTree;
        previousDirect = directSeconds;
        previousTree = treeSeconds;
    }

    // Measured rather than asserted. Each doubling of N should multiply the
    // direct cost by about 4 (quadratic) and the tree cost by about 2.
    std::printf("\nCost per doubling of N at the top of the range: direct x%.1f, "
                "Barnes-Hut x%.1f\n", directGrowth, treeGrowth);
    std::printf("(x4 is quadratic, x2 is N log N.)\n");
    if (crossover > 0) {
        std::printf("Barnes-Hut overtakes direct summation at about %d bodies; "
                    "best measured speedup %.1fx at %d.\n", crossover, bestSpeedup,
                    bestSpeedupCount);
    } else {
        std::printf("Direct summation still wins at every size tested; the tree "
                    "build dominates below the crossover.\n");
    }
    std::printf("\n");
    return 0;
}

}  // namespace engine
