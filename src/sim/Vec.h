#pragma once

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

namespace sim {

// The simulation is double precision throughout. Earth's orbital radius is
// ~1.5e11 m; float carries ~7 significant digits, which would quantise Earth's
// position into steps of roughly 10 000 km. Conversion to float happens once,
// in the renderer, after subtracting the camera origin.
using Vec3 = glm::dvec3;

inline double lengthSquared(const Vec3& v) { return glm::dot(v, v); }

// glm::normalize on a zero vector yields NaN; coincident bodies are a real case
// here (a body spawned exactly on another), so guard it.
inline Vec3 safeNormalize(const Vec3& v, const Vec3& fallback = Vec3(0.0)) {
    const double lengthSq = lengthSquared(v);
    if (lengthSq <= 0.0) return fallback;
    return v / std::sqrt(lengthSq);
}

}  // namespace sim
