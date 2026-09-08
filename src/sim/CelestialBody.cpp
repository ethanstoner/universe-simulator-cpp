#include "sim/CelestialBody.h"

#include <cmath>

namespace sim {
namespace {
constexpr double kPi = 3.14159265358979323846;
}

double CelestialBody::density() const {
    if (radius <= 0.0) return 0.0;
    const double volume = (4.0 / 3.0) * kPi * radius * radius * radius;
    return mass / volume;
}

void CelestialBody::setRadiusFromDensity(double density) {
    if (density <= 0.0 || mass <= 0.0) {
        radius = 0.0;
        return;
    }
    const double volume = mass / density;
    radius = std::cbrt(volume * 3.0 / (4.0 * kPi));
}

}  // namespace sim
