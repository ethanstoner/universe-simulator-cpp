#include "TestFramework.h"

#include "sim/Constants.h"
#include "sim/Units.h"

using namespace sim;

TEST(units_formats_distance_in_km) {
    CHECK(formatDistance(constants::kEarthRadius) == "6371.000 km");
}

TEST(units_formats_distance_in_au) {
    CHECK(formatDistance(constants::kAu) == "1.0000 AU");
}

TEST(units_formats_mass_in_solar_masses) {
    CHECK(formatMass(constants::kSolarMass) == "1.0000 M_sun");
}

TEST(units_formats_mass_in_earth_masses) {
    CHECK(formatMass(constants::kEarthMass) == "1.0000 M_earth");
}

TEST(units_formats_duration_in_years) {
    CHECK(formatDuration(constants::kJulianYear) == "1.0000 yr");
    CHECK(formatDuration(constants::kDay) == "1.000 d");
    CHECK(formatDuration(90.0) == "1.500 min");
}

TEST(units_formats_speed_in_km_per_second) {
    CHECK(formatSpeed(29780.0) == "29.7800 km/s");
}
