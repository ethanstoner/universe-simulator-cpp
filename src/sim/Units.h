#pragma once

#include <string>

namespace sim {

// Human-readable formatting for the UI. The simulation itself never uses these;
// they exist purely so that "1.49597870700e11 m" reads as "1.000 AU".
std::string formatDistance(double metres);
std::string formatMass(double kilograms);
std::string formatDuration(double seconds);
std::string formatSpeed(double metresPerSecond);
std::string formatEnergy(double joules);
std::string formatScientific(double value, const char* unit, int significantDigits = 4);

}  // namespace sim
