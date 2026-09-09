#include "TestFramework.h"

#include <cmath>
#include <cstdio>

#include "sim/Constants.h"
#include "sim/Diagnostics.h"
#include "sim/GravitySystem.h"
#include "sim/OrbitMath.h"

using namespace sim;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kRadiansToArcsec = 180.0 / kPi * 3600.0;

// Mercury, from the NASA fact sheet.
constexpr double kMercurySemiMajorAxis = 5.790905e10;  // m
constexpr double kMercuryEccentricity = 0.205630;
constexpr double kMercuryMass = 3.3011e23;

// Sun plus one planet, started at perihelion on the +X axis so the initial
// periapsis direction is exactly zero and any rotation is the effect measured.
GravitySystem makeTwoBody(double semiMajorAxis, double eccentricity,
                          double planetMass, bool relativistic) {
    GravitySystem system;
    system.settings().stabilization = Stabilization::None;
    system.settings().trailLength = 0;
    system.settings().relativisticCorrection = relativistic;

    CelestialBody sun;
    sun.name = "Sun";
    sun.mass = constants::kSolarMass;
    sun.radius = 6.9634e8;
    sun.fixed = true;  // removes the barycentre wobble from the measurement
    sun.showTrail = false;
    system.add(sun);

    CelestialBody planet;
    planet.name = "Planet";
    planet.mass = planetMass;
    planet.radius = 2.4e6;
    planet.showTrail = false;
    planet.position = Vec3(perihelionDistance(semiMajorAxis, eccentricity), 0.0, 0.0);
    planet.velocity =
        Vec3(0.0, 0.0, perihelionSpeed(constants::kSolarMass, semiMajorAxis, eccentricity));
    system.add(planet);
    return system;
}

// Integrates for `seconds` and returns the total rotation of the periapsis
// direction, unwrapped so it accumulates past a full turn rather than folding
// back into [-pi, pi].
double measurePrecession(GravitySystem& system, double seconds, double dt) {
    const double mu = constants::kG * constants::kSolarMass;

    auto currentAngle = [&]() {
        const CelestialBody& sun = system.bodies()[0];
        const CelestialBody& planet = system.bodies()[1];
        return periapsisAngle(planet.position - sun.position,
                              planet.velocity - sun.velocity, mu);
    };

    double previous = currentAngle();
    double accumulated = 0.0;
    const long long steps = static_cast<long long>(seconds / dt);

    for (long long i = 0; i < steps; ++i) {
        system.step(dt);
        const double angle = currentAngle();
        double delta = angle - previous;
        // Unwrap: a jump larger than half a turn is the atan2 branch cut, not
        // real motion.
        if (delta > kPi) delta -= 2.0 * kPi;
        if (delta < -kPi) delta += 2.0 * kPi;
        accumulated += delta;
        previous = angle;
    }
    return accumulated;
}

}  // namespace

TEST(relativity_perihelion_formula_gives_43_arcsec_for_mercury) {
    // The closed form first, so a failure in the simulation can be told apart
    // from a failure in the number it is being compared against.
    const double perOrbit = relativisticPerihelionAdvance(
        constants::kSolarMass, kMercurySemiMajorAxis, kMercuryEccentricity);
    const double period = orbitalPeriod(constants::kSolarMass, kMercurySemiMajorAxis);
    const double orbitsPerCentury = 100.0 * constants::kJulianYear / period;
    const double arcsecPerCentury = perOrbit * orbitsPerCentury * kRadiansToArcsec;

    CHECK_NEAR(period / constants::kDay, 87.969, 0.05);  // Mercury's year
    CHECK_NEAR(arcsecPerCentury, 43.0, 0.5);
}

TEST(relativity_newtonian_precession_is_numerical_and_scales_as_dt_squared) {
    // The control, and the reason the headline measurement below is done by
    // differencing rather than directly.
    //
    // A Kepler ellipse is closed, so a Newtonian two-body orbit should show no
    // precession at all. It does: velocity Verlet conserves a shadow
    // Hamiltonian whose orbit precesses, and at a 600 s step on Mercury's
    // eccentricity that artificial drift is about -38 arcsec/century --
    // comparable in size to the 43 the relativistic term produces, and
    // retrograde where the real effect is prograde.
    //
    // That is truncation error rather than a bug, and the way to show it is to
    // refine the step: a second-order method's error should fall by roughly
    // four for each halving.
    auto driftArcsecPerCentury = [](double dt, double years) {
        GravitySystem system = makeTwoBody(kMercurySemiMajorAxis, kMercuryEccentricity,
                                           kMercuryMass, /*relativistic=*/false);
        const double drift = measurePrecession(system, years * constants::kJulianYear, dt);
        return drift * kRadiansToArcsec * (100.0 / years);
    };

    const double years = 4.0;
    const double coarse = driftArcsecPerCentury(1200.0, years);
    const double medium = driftArcsecPerCentury(600.0, years);
    const double fine = driftArcsecPerCentury(300.0, years);

    std::printf("         Newtonian numerical drift: %+.2f (dt=1200 s), "
                "%+.2f (600 s), %+.2f (300 s) arcsec/century\n",
                coarse, medium, fine);

    // Monotonically towards zero, and roughly a factor of four per halving.
    CHECK_LESS(std::abs(medium), std::abs(coarse));
    CHECK_LESS(std::abs(fine), std::abs(medium));
    const double ratio = std::abs(coarse) / std::abs(fine);
    CHECK(ratio > 6.0);   // two halvings of a second-order method: about 16x
    CHECK(ratio < 40.0);
}

TEST(relativity_reproduces_mercurys_perihelion_precession) {
    // The headline check: switch the 1PN term on and measure the advance
    // numerically. This is the classic test of general relativity, and getting
    // 43 arcsec per century out of an integrator is a strong signal that both
    // the correction and the integration are right.
    const double years = 20.0;

    GravitySystem newtonian = makeTwoBody(kMercurySemiMajorAxis, kMercuryEccentricity,
                                          kMercuryMass, false);
    GravitySystem relativistic = makeTwoBody(kMercurySemiMajorAxis, kMercuryEccentricity,
                                             kMercuryMass, true);

    const double dt = 600.0;
    const double seconds = years * constants::kJulianYear;
    const double baseline = measurePrecession(newtonian, seconds, dt);
    const double withCorrection = measurePrecession(relativistic, seconds, dt);

    // Subtracting the Newtonian run removes whatever numerical drift the
    // integrator contributes, leaving the physical effect.
    const double measured =
        (withCorrection - baseline) * kRadiansToArcsec * (100.0 / years);
    const double expected =
        relativisticPerihelionAdvance(constants::kSolarMass, kMercurySemiMajorAxis,
                                      kMercuryEccentricity) *
        (100.0 * constants::kJulianYear /
         orbitalPeriod(constants::kSolarMass, kMercurySemiMajorAxis)) *
        kRadiansToArcsec;

    std::printf("         Mercury precession: measured %.2f, expected %.2f "
                "arcsec/century\n", measured, expected);

    CHECK(measured > 0.0);  // prograde, as observed
    CHECK_NEAR(measured, expected, 1.5);
    CHECK_NEAR(measured, 43.0, 2.0);
}

TEST(relativity_advance_scales_as_the_closed_form_predicts) {
    // 6 pi G M / (c^2 a (1 - e^2)) says the advance per orbit falls with
    // semi-major axis. Measuring two different orbits and comparing the ratio
    // tests the correction's shape, not just one number.
    auto advancePerOrbit = [](double semiMajorAxis, double eccentricity) {
        GravitySystem newtonian =
            makeTwoBody(semiMajorAxis, eccentricity, 1.0e20, false);
        GravitySystem relativistic =
            makeTwoBody(semiMajorAxis, eccentricity, 1.0e20, true);

        const double period = orbitalPeriod(constants::kSolarMass, semiMajorAxis);
        const double seconds = 30.0 * period;
        const double dt = period / 20000.0;
        const double difference = measurePrecession(relativistic, seconds, dt) -
                                  measurePrecession(newtonian, seconds, dt);
        return difference / 30.0;
    };

    const double inner = advancePerOrbit(kMercurySemiMajorAxis, kMercuryEccentricity);
    const double outer = advancePerOrbit(2.0 * kMercurySemiMajorAxis, kMercuryEccentricity);

    const double predictedInner = relativisticPerihelionAdvance(
        constants::kSolarMass, kMercurySemiMajorAxis, kMercuryEccentricity);
    const double predictedOuter = relativisticPerihelionAdvance(
        constants::kSolarMass, 2.0 * kMercurySemiMajorAxis, kMercuryEccentricity);

    CHECK_REL(inner, predictedInner, 0.05);
    CHECK_REL(outer, predictedOuter, 0.05);
    // Doubling a halves the advance per orbit.
    CHECK_NEAR(inner / outer, 2.0, 0.15);
}

TEST(relativity_correction_is_negligible_at_solar_system_scale) {
    // It must not quietly change ordinary results. Over a decade of Earth's
    // orbit the correction should move nothing measurable.
    auto earthRadiusAfterDecade = [](bool relativistic) {
        GravitySystem system = makeTwoBody(constants::kAu, 0.0167,
                                           constants::kEarthMass, relativistic);
        const double dt = 3600.0;
        const long long steps =
            static_cast<long long>(10.0 * constants::kJulianYear / dt);
        for (long long i = 0; i < steps; ++i) system.step(dt);
        return glm::length(system.bodies()[1].position);
    };

    const double newtonian = earthRadiusAfterDecade(false);
    const double relativistic = earthRadiusAfterDecade(true);
    CHECK_LESS(std::abs(relativistic - newtonian) / newtonian, 1e-6);
}

TEST(relativity_is_off_by_default) {
    // The simulator is Newtonian unless explicitly told otherwise.
    GravitySystem system;
    CHECK(!system.settings().relativisticCorrection);
    CHECK(system.settings().relativisticStrength == 1.0);
}
