#include "sim/BarnesHut.h"

#include <algorithm>
#include <cmath>

namespace sim {
namespace {

// Recursion guard. Bodies that are exactly coincident can never be separated by
// subdivision, so without a cap the build would recurse until the stack runs
// out. At the cap the remaining bodies share a leaf, which is the right answer:
// they are treated as one aggregate mass, which is what they effectively are.
constexpr int kMaxDepth = 32;

int octantOf(const Vec3& point, const Vec3& centre) {
    return (point.x >= centre.x ? 1 : 0) | (point.y >= centre.y ? 2 : 0) |
           (point.z >= centre.z ? 4 : 0);
}

Vec3 childCentre(const Vec3& centre, double halfWidth, int octant) {
    const double quarter = halfWidth * 0.5;
    return Vec3(centre.x + ((octant & 1) ? quarter : -quarter),
                centre.y + ((octant & 2) ? quarter : -quarter),
                centre.z + ((octant & 4) ? quarter : -quarter));
}

}  // namespace

// Fills the already-allocated node at `self`. Children are allocated in one
// contiguous block of eight and filled by recursion, so no node is ever built
// somewhere temporary and copied: every index stays valid for the whole build.
void BarnesHutTree::buildRecursive(int self, const std::vector<int>& indices,
                                   const Vec3& centre, double halfWidth, int depth) {
    depth_ = std::max(depth_, depth);

    {
        Node& node = nodes_[self];
        node.centre = centre;
        node.halfWidth = halfWidth;
        node.firstChild = -1;
        node.bodyIndex = -1;
        node.count = static_cast<int>(indices.size());
        node.mass = 0.0;
        node.centreOfMass = centre;
    }
    if (indices.empty()) return;

    double mass = 0.0;
    Vec3 weighted(0.0);
    for (int index : indices) {
        const double m = (*masses_)[index];
        mass += m;
        weighted += (*positions_)[index] * m;
    }
    nodes_[self].mass = mass;
    if (mass > 0.0) nodes_[self].centreOfMass = weighted / mass;

    if (indices.size() == 1) {
        nodes_[self].bodyIndex = indices[0];
        return;
    }
    if (depth >= kMaxDepth) return;  // coincident bodies: keep as one leaf

    std::vector<int> octants[8];
    for (int index : indices) {
        octants[octantOf((*positions_)[index], centre)].push_back(index);
    }

    const int firstChild = static_cast<int>(nodes_.size());
    nodes_.resize(nodes_.size() + 8);
    nodes_[self].firstChild = firstChild;

    for (int octant = 0; octant < 8; ++octant) {
        buildRecursive(firstChild + octant, octants[octant],
                       childCentre(centre, halfWidth, octant), halfWidth * 0.5,
                       depth + 1);
    }
}

void BarnesHutTree::build(const std::vector<Vec3>& positions,
                          const std::vector<double>& masses) {
    nodes_.clear();
    depth_ = 0;
    positions_ = &positions;
    masses_ = &masses;

    std::vector<int> indices;
    indices.reserve(positions.size());
    for (std::size_t i = 0; i < positions.size() && i < masses.size(); ++i) {
        if (masses[i] > 0.0) indices.push_back(static_cast<int>(i));
    }
    if (indices.empty()) return;

    // A cube enclosing every body. Cubic rather than a tight box so the opening
    // criterion s/d has a single well-defined width per level.
    Vec3 low = positions[indices[0]];
    Vec3 high = low;
    for (int index : indices) {
        low = glm::min(low, positions[index]);
        high = glm::max(high, positions[index]);
    }
    const Vec3 centre = (low + high) * 0.5;
    const Vec3 extent = high - low;
    double halfWidth = 0.5 * std::max({extent.x, extent.y, extent.z});
    // Nudged outwards so a body exactly on the boundary still lands inside.
    halfWidth = halfWidth > 0.0 ? halfWidth * 1.0000001 : 1.0;

    nodes_.reserve(indices.size() * 4);
    nodes_.resize(1);
    buildRecursive(0, indices, centre, halfWidth, 0);
}

Vec3 BarnesHutTree::accelerationAt(const Vec3& point, int ignoreBody, double G,
                                   double theta, double softeningSquared) const {
    if (nodes_.empty()) return Vec3(0.0);

    Vec3 acceleration(0.0);
    // Explicit stack rather than recursion: this is the hot path. Depth is
    // capped at kMaxDepth and each visit pushes at most 8, so the bound is
    // 8 * kMaxDepth with headroom.
    constexpr int kStackSize = 8 * kMaxDepth + 16;
    int stack[kStackSize];
    int top = 0;
    stack[top++] = 0;

    const double thetaSquared = theta * theta;

    while (top > 0) {
        const Node& node = nodes_[stack[--top]];
        if (node.mass <= 0.0 || node.count == 0) continue;

        // A single-body leaf that is the body being solved for contributes
        // nothing, and including it would divide by zero.
        if (node.bodyIndex >= 0 && node.bodyIndex == ignoreBody) continue;

        const bool isLeaf = node.firstChild < 0;

        // An aggregate that might contain the ignored body cannot be accepted
        // wholesale: its centre of mass includes that body. Descend instead.
        if (!isLeaf && ignoreBody >= 0) {
            const Vec3& ignored = (*positions_)[ignoreBody];
            const bool mayContain =
                std::abs(ignored.x - node.centre.x) <= node.halfWidth &&
                std::abs(ignored.y - node.centre.y) <= node.halfWidth &&
                std::abs(ignored.z - node.centre.z) <= node.halfWidth;
            if (mayContain) {
                if (top + 8 <= kStackSize) {
                    for (int i = 0; i < 8; ++i) stack[top++] = node.firstChild + i;
                }
                continue;
            }
        }

        const Vec3 delta = node.centreOfMass - point;
        const double distanceSquared = lengthSquared(delta) + softeningSquared;

        // Opening criterion: accept the aggregate when s/d < theta, i.e.
        // s^2 < theta^2 d^2. Squared to avoid a square root per test.
        const double width = node.halfWidth * 2.0;
        const bool farEnough = width * width < thetaSquared * distanceSquared;

        if (isLeaf || farEnough) {
            if (distanceSquared <= 0.0) continue;
            acceleration +=
                delta * (G * node.mass / (distanceSquared * std::sqrt(distanceSquared)));
            continue;
        }

        if (top + 8 <= kStackSize) {
            for (int i = 0; i < 8; ++i) stack[top++] = node.firstChild + i;
        }
    }
    return acceleration;
}

}  // namespace sim
