#include "sim/Units.h"

#include <cmath>
#include <cstdio>

#include "sim/Constants.h"

namespace sim {
namespace {

std::string printf_(const char* format, double value, const char* unit) {
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), format, value, unit);
    return buffer;
}

}  // namespace

std::string formatScientific(double value, const char* unit, int significantDigits) {
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "%.*g %s", significantDigits, value, unit);
    return buffer;
}

std::string formatDistance(double metres) {
    const double magnitude = std::abs(metres);
    if (magnitude >= 0.05 * constants::kAu) {
        return printf_("%.4f %s", metres / constants::kAu, "AU");
    }
    // km all the way up to the AU threshold: astronomy quotes planetary radii
    // and orbital distances in kilometres, never in megametres.
    if (magnitude >= 1.0e3) return printf_("%.3f %s", metres / 1.0e3, "km");
    if (magnitude == 0.0) return "0 m";
    if (magnitude < 1.0e-2) return formatScientific(metres, "m");
    return printf_("%.3f %s", metres, "m");
}

std::string formatMass(double kilograms) {
    const double magnitude = std::abs(kilograms);
    if (magnitude >= 0.001 * constants::kSolarMass) {
        return printf_("%.4f %s", kilograms / constants::kSolarMass, "M_sun");
    }
    if (magnitude >= 0.001 * constants::kEarthMass) {
        return printf_("%.4f %s", kilograms / constants::kEarthMass, "M_earth");
    }
    if (magnitude >= 1.0e9) return formatScientific(kilograms, "kg");
    return printf_("%.4g %s", kilograms, "kg");
}

std::string formatDuration(double seconds) {
    const double magnitude = std::abs(seconds);
    if (magnitude >= constants::kJulianYear) {
        return printf_("%.4f %s", seconds / constants::kJulianYear, "yr");
    }
    if (magnitude >= constants::kDay) {
        return printf_("%.3f %s", seconds / constants::kDay, "d");
    }
    if (magnitude >= 3600.0) return printf_("%.3f %s", seconds / 3600.0, "h");
    if (magnitude >= 60.0) return printf_("%.3f %s", seconds / 60.0, "min");
    return printf_("%.3f %s", seconds, "s");
}

std::string formatSpeed(double metresPerSecond) {
    const double magnitude = std::abs(metresPerSecond);
    if (magnitude >= 1.0e3) return printf_("%.4f %s", metresPerSecond / 1.0e3, "km/s");
    return printf_("%.4f %s", metresPerSecond, "m/s");
}

std::string formatEnergy(double joules) { return formatScientific(joules, "J"); }

}  // namespace sim
