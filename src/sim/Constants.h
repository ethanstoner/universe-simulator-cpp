#pragma once

namespace sim {

// Everything in the simulation core is SI: metres, kilograms, seconds.
// Rendering applies its own scaling; see docs/ARCHITECTURE.md "Scales".
namespace constants {

inline constexpr double kG = 6.67430e-11;          // m^3 kg^-1 s^-2 (CODATA 2018)
inline constexpr double kC = 299792458.0;          // m/s, exact
inline constexpr double kAu = 149597870700.0;      // m, exact by definition
inline constexpr double kSolarMass = 1.98892e30;   // kg
inline constexpr double kEarthMass = 5.97219e24;   // kg
inline constexpr double kEarthRadius = 6371000.0;  // m, volumetric mean
inline constexpr double kDay = 86400.0;            // s
inline constexpr double kJulianYear = 31557600.0;  // s (365.25 days)
inline constexpr double kEarthSurfaceGravity = 9.81;  // m/s^2

}  // namespace constants
}  // namespace sim
