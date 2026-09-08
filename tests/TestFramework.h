#pragma once

// A ~100 line test runner. The simulation core has no third-party dependencies
// and neither do its tests; adding gtest here would be more build surface than
// the assertions are worth.

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace testing {

struct TestCase {
    std::string name;
    std::function<void()> body;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> cases;
    return cases;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> body) {
        registry().push_back({name, std::move(body)});
    }
};

struct Failure {
    std::string message;
};

inline void fail(const std::string& message) { throw Failure{message}; }

inline void expectTrue(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        fail(std::string(file) + ":" + std::to_string(line) + "  expected true: " + expression);
    }
}

inline void expectNear(double actual, double expected, double tolerance,
                       const char* expression, const char* file, int line) {
    const double difference = std::abs(actual - expected);
    if (!(difference <= tolerance) || std::isnan(actual)) {
        char buffer[512];
        std::snprintf(buffer, sizeof(buffer),
                      "%s:%d  %s\n      actual   %.17g\n      expected %.17g\n"
                      "      |diff|   %.6g > tol %.6g",
                      file, line, expression, actual, expected, difference, tolerance);
        fail(buffer);
    }
}

// Relative comparison, which is what most physics quantities want. Note the
// division is by the signed expected value: dividing by |expected| would report
// a perfect match as -1 whenever the expected value is negative, which silently
// inverted every test involving potential energy or an inward acceleration.
inline void expectRelative(double actual, double expected, double relativeTolerance,
                           const char* expression, const char* file, int line) {
    if (expected == 0.0) {
        expectNear(actual, 0.0, relativeTolerance, expression, file, line);
        return;
    }
    const double ratio = actual / expected;
    if (!(std::abs(ratio - 1.0) <= relativeTolerance) || std::isnan(ratio)) {
        char buffer[512];
        std::snprintf(buffer, sizeof(buffer),
                      "%s:%d  %s\n      actual   %.17g\n      expected %.17g\n"
                      "      rel err  %.6g > tol %.6g",
                      file, line, expression, actual, expected,
                      std::abs(ratio - 1.0), relativeTolerance);
        fail(buffer);
    }
}

// Reports both operands on failure, which matters for "is this drift small
// enough" assertions where the interesting information is the actual number.
inline void expectLess(double actual, double limit, const char* expression,
                       const char* file, int line) {
    if (!(actual < limit) || std::isnan(actual)) {
        char buffer[512];
        std::snprintf(buffer, sizeof(buffer),
                      "%s:%d  %s\n      actual %.17g\n      limit  %.17g",
                      file, line, expression, actual, limit);
        fail(buffer);
    }
}

inline int runAll(int argc, char** argv) {
    const std::string filter = argc > 1 ? argv[1] : "";
    int passed = 0;
    std::vector<std::string> failures;

    for (const TestCase& test : registry()) {
        if (!filter.empty() && test.name.find(filter) == std::string::npos) continue;
        try {
            test.body();
            std::printf("  [ ok ] %s\n", test.name.c_str());
            ++passed;
        } catch (const Failure& failure) {
            std::printf("  [FAIL] %s\n    %s\n", test.name.c_str(), failure.message.c_str());
            failures.push_back(test.name);
        } catch (const std::exception& error) {
            std::printf("  [FAIL] %s\n    threw: %s\n", test.name.c_str(), error.what());
            failures.push_back(test.name);
        }
    }

    std::printf("\n%d passed, %zu failed\n", passed, failures.size());
    for (const std::string& name : failures) std::printf("  failed: %s\n", name.c_str());
    return failures.empty() ? 0 : 1;
}

}  // namespace testing

#define TEST(name)                                                            \
    static void name();                                                       \
    static ::testing::Registrar registrar_##name(#name, name);                \
    static void name()

#define CHECK(expr) ::testing::expectTrue((expr), #expr, __FILE__, __LINE__)
#define CHECK_NEAR(actual, expected, tol) \
    ::testing::expectNear((actual), (expected), (tol), #actual " ~= " #expected, __FILE__, __LINE__)
#define CHECK_REL(actual, expected, tol) \
    ::testing::expectRelative((actual), (expected), (tol), #actual " ~= " #expected, __FILE__, __LINE__)
#define CHECK_LESS(actual, limit) \
    ::testing::expectLess((actual), (limit), #actual " < " #limit, __FILE__, __LINE__)
