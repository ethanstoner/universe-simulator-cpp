#pragma once

#include <vector>

#include "sim/CelestialBody.h"
#include "sim/Vec.h"

namespace sim {

// Barnes-Hut octree for approximate N-body gravity.
//
// Direct summation is O(N^2), which is fine for the tens of bodies a solar
// system needs but not for the thousands an asteroid belt wants. Barnes-Hut
// groups distant bodies: a cube of width s whose centre of mass is a distance d
// away is treated as a single point mass when s/d < theta.
//
// The cost is accuracy, and the trade is explicit:
//
//   theta = 0    every leaf is visited; identical to direct summation
//   theta ~ 0.5  the usual choice; sub-percent force error
//   theta large  faster and progressively wronger
//
// It also breaks Newton's third law. Body A may approximate a distant cluster
// while a body inside that cluster resolves A exactly, so the two forces are no
// longer exactly equal and opposite and total momentum is conserved only to the
// approximation error. Direct summation remains the default for that reason,
// and the UI says so. See docs/PHYSICS.md.
class BarnesHutTree {
public:
    BarnesHutTree() = default;

    // The tree holds raw pointers into the caller's position and mass arrays
    // for the duration of a build. Copying it would carry those pointers into
    // the copy, where they would refer to the *original* owner's storage --
    // which is exactly what happens when a GravitySystem is copied to forecast
    // a trajectory. It is a cache, so a copy simply starts empty and is rebuilt
    // on the next force evaluation.
    BarnesHutTree(const BarnesHutTree&) {}
    BarnesHutTree& operator=(const BarnesHutTree&) {
        nodes_.clear();
        positions_ = nullptr;
        masses_ = nullptr;
        depth_ = 0;
        return *this;
    }
    BarnesHutTree(BarnesHutTree&&) = default;
    BarnesHutTree& operator=(BarnesHutTree&&) = default;

    // Rebuilds from scratch. Bodies with non-positive mass are ignored.
    void build(const std::vector<Vec3>& positions, const std::vector<double>& masses);

    // Acceleration at `point` from every mass in the tree, optionally skipping
    // one body (its own contribution, which would be a division by zero).
    Vec3 accelerationAt(const Vec3& point, int ignoreBody, double G, double theta,
                        double softeningSquared) const;

    bool empty() const { return nodes_.empty(); }
    std::size_t nodeCount() const { return nodes_.size(); }
    int depth() const { return depth_; }

    // Total mass and centre of mass of the root, exposed for testing.
    double totalMass() const { return nodes_.empty() ? 0.0 : nodes_[0].mass; }
    Vec3 centreOfMass() const {
        return nodes_.empty() ? Vec3(0.0) : nodes_[0].centreOfMass;
    }

private:
    struct Node {
        Vec3 centre{0.0};        // geometric centre of this cube
        double halfWidth = 0.0;  // half the cube's edge length
        Vec3 centreOfMass{0.0};
        double mass = 0.0;
        // Children are stored contiguously: firstChild .. firstChild + 7.
        // -1 means this is a leaf.
        int firstChild = -1;
        int bodyIndex = -1;  // set only for single-body leaves
        int count = 0;
    };

    // Fills the already-allocated node at `self`.
    void buildRecursive(int self, const std::vector<int>& indices, const Vec3& centre,
                        double halfWidth, int depth);

    const std::vector<Vec3>* positions_ = nullptr;
    const std::vector<double>* masses_ = nullptr;
    std::vector<Node> nodes_;
    int depth_ = 0;
};

}  // namespace sim
