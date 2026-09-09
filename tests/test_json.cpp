#include "TestFramework.h"

#include <cmath>

#include "sim/Constants.h"
#include "sim/Json.h"
#include "sim/SceneConfig.h"
#include "sim/SceneLibrary.h"

using namespace sim;

// ------------------------------------------------------------------ the parser

TEST(json_parses_scalars) {
    Json value;
    std::string error;
    CHECK(Json::parse("42.5", value, error));
    CHECK_NEAR(value.asNumber(), 42.5, 1e-15);

    CHECK(Json::parse("true", value, error));
    CHECK(value.asBool());

    CHECK(Json::parse("\"hello\"", value, error));
    CHECK(value.asString() == "hello");

    CHECK(Json::parse("null", value, error));
    CHECK(value.isNull());
}

TEST(json_parses_nested_structures) {
    Json value;
    std::string error;
    const char* text = R"({"a": [1, 2, {"b": "c"}], "d": {"e": false}})";
    CHECK(Json::parse(text, value, error));
    CHECK(value.isObject());
    CHECK(value["a"].isArray());
    CHECK(value["a"].size() == 3);
    CHECK_NEAR(value["a"][1].asNumber(), 2.0, 1e-15);
    CHECK(value["a"][2]["b"].asString() == "c");
    CHECK(value["d"]["e"].asBool(true) == false);
}

TEST(json_handles_escapes) {
    Json value;
    std::string error;
    CHECK(Json::parse(R"("line\nbreak \"quoted\" back\\slash")", value, error));
    CHECK(value.asString() == "line\nbreak \"quoted\" back\\slash");
}

TEST(json_rejects_malformed_documents) {
    Json value;
    std::string error;
    CHECK(!Json::parse("{\"a\": }", value, error));
    CHECK(!error.empty());
    CHECK(!Json::parse("[1, 2", value, error));
    CHECK(!Json::parse("{\"a\": 1} trailing", value, error));
    CHECK(!Json::parse("", value, error));
}

TEST(json_missing_keys_yield_defaults_rather_than_throwing) {
    Json value;
    std::string error;
    CHECK(Json::parse(R"({"present": 1})", value, error));
    CHECK_NEAR(value["absent"].asNumber(7.0), 7.0, 1e-15);
    CHECK(value["absent"]["deeper"].asString("fallback") == "fallback");
    CHECK(!value.contains("absent"));
    CHECK(value.contains("present"));
}

TEST(json_round_trips_doubles_exactly) {
    // Initial conditions are stored in these files, so a save/load cycle has to
    // reproduce the same trajectory bit for bit.
    const double values[] = {constants::kAu, constants::kSolarMass, constants::kG,
                             1.0 / 3.0, -29784.123456789, 1e-300, 6.371e6};
    for (double original : values) {
        Json object = Json::object();
        object.set("v", Json(original));

        Json reparsed;
        std::string error;
        CHECK(Json::parse(object.dump(), reparsed, error));
        CHECK(reparsed["v"].asNumber() == original);
    }
}

TEST(json_writer_produces_reparseable_output) {
    Json root = Json::object();
    root.set("name", Json("Sun"));
    root.set("mass", Json(constants::kSolarMass));
    Json array = Json::array();
    array.push(Json(1.0));
    array.push(Json(2.0));
    root.set("position", array);
    root.set("empty_object", Json::object());
    root.set("empty_array", Json::array());

    Json reparsed;
    std::string error;
    CHECK(Json::parse(root.dump(), reparsed, error));
    CHECK(reparsed["name"].asString() == "Sun");
    CHECK(reparsed["position"][1].asNumber() == 2.0);
    CHECK(reparsed["empty_array"].isArray());
}

// ------------------------------------------------------------- scene configs

TEST(config_round_trips_every_builtin_scene) {
    for (const Scene& original : builtinScenes()) {
        Scene restored;
        std::string error;
        const bool ok = sceneFromJson(sceneToJson(original), restored, error);
        if (!ok) ::testing::fail(original.key + ": " + error);

        CHECK(restored.key == original.key);
        CHECK(restored.title == original.title);
        CHECK(restored.bodies.size() == original.bodies.size());
        CHECK(restored.fixedTimeStep == original.fixedTimeStep);
        CHECK(restored.settings.integrator == original.settings.integrator);
        CHECK(restored.settings.stabilization == original.settings.stabilization);
        CHECK(restored.settings.collisionMode == original.settings.collisionMode);
        CHECK(restored.settings.trailLength == original.settings.trailLength);
        CHECK(restored.view.metresPerUnit == original.view.metresPerUnit);
        CHECK(restored.view.trueScale == original.view.trueScale);
        CHECK(restored.view.focusBody == original.view.focusBody);
        CHECK(restored.view.trailReferenceBody == original.view.trailReferenceBody);

        for (std::size_t i = 0; i < original.bodies.size(); ++i) {
            const CelestialBody& a = original.bodies[i];
            const CelestialBody& b = restored.bodies[i];
            CHECK(a.name == b.name);
            // Exact equality, not approximate: these are initial conditions.
            CHECK(a.mass == b.mass);
            CHECK(a.radius == b.radius);
            CHECK(a.position == b.position);
            CHECK(a.velocity == b.velocity);
            CHECK(a.emissive == b.emissive);
            CHECK(a.fixed == b.fixed);
        }
    }
}

TEST(config_round_trip_reproduces_the_same_trajectory) {
    const Scene* original = findScene("inner");
    Scene restored;
    std::string error;
    CHECK(sceneFromJson(sceneToJson(*original), restored, error));

    auto advance = [](const Scene& scene) {
        GravitySystem system;
        applyScene(scene, system);
        for (int i = 0; i < 4000; ++i) system.step(1800.0);
        Vec3 total(0.0);
        for (const CelestialBody& body : system.bodies()) total += body.position;
        return total;
    };

    const Vec3 a = advance(*original);
    const Vec3 b = advance(restored);
    CHECK(a.x == b.x);
    CHECK(a.y == b.y);
    CHECK(a.z == b.z);
}

TEST(config_rejects_scenes_without_a_key_or_bodies) {
    Scene scene;
    std::string error;
    CHECK(!sceneFromJson(R"({"bodies": [{"name": "a"}]})", scene, error));
    CHECK(!error.empty());
    CHECK(!sceneFromJson(R"({"key": "x", "bodies": []})", scene, error));
    CHECK(!sceneFromJson(R"({"key": "x"})", scene, error));
}

TEST(config_rejects_a_non_positive_timestep) {
    Scene scene;
    std::string error;
    const char* text = R"({
      "key": "x", "fixedTimeStepSeconds": 0,
      "view": {"metresPerUnit": 1},
      "bodies": [{"name": "a", "massKg": 1, "radiusMetres": 1}]
    })";
    CHECK(!sceneFromJson(text, scene, error));
}

TEST(config_rejects_negative_mass) {
    Scene scene;
    std::string error;
    const char* text = R"({
      "key": "x",
      "view": {"metresPerUnit": 1},
      "bodies": [{"name": "a", "massKg": -5, "radiusMetres": 1}]
    })";
    CHECK(!sceneFromJson(text, scene, error));
}

TEST(config_loads_a_minimal_hand_written_scene) {
    // What a user would plausibly type by hand: only the fields they care
    // about, with everything else defaulted.
    const char* text = R"({
      "key": "hand-written",
      "title": "Two suns",
      "view": {"metresPerUnit": 1.5e10, "cameraDistance": 30},
      "bodies": [
        {"name": "A", "massKg": 2e30, "radiusMetres": 7e8,
         "positionMetres": [-7.5e10, 0, 0], "velocityMetresPerSecond": [0, 0, -15000]},
        {"name": "B", "massKg": 2e30, "radiusMetres": 7e8,
         "positionMetres": [7.5e10, 0, 0], "velocityMetresPerSecond": [0, 0, 15000]}
      ]
    })";
    Scene scene;
    std::string error;
    const bool ok = sceneFromJson(text, scene, error);
    if (!ok) ::testing::fail(error);

    CHECK(scene.key == "hand-written");
    CHECK(scene.bodies.size() == 2);
    CHECK(scene.bodies[0].mass == 2e30);
    CHECK(scene.settings.integrator == IntegratorType::VelocityVerlet);
    CHECK(scene.settings.pairwiseGravityEnabled);

    // And it must actually run.
    GravitySystem system;
    applyScene(scene, system);
    for (int i = 0; i < 500; ++i) system.step(3600.0);
    CHECK(std::isfinite(system.bodies()[0].position.x));
    CHECK(glm::length(system.bodies()[0].position) < 1e13);
}

TEST(config_tolerates_line_comments) {
    const char* text = R"({
      // A hand-edited file is much more useful with comments.
      "key": "commented",
      "view": {"metresPerUnit": 1},
      "bodies": [{"name": "a", "massKg": 1, "radiusMetres": 1}]
    })";
    Scene scene;
    std::string error;
    const bool ok = sceneFromJson(text, scene, error);
    if (!ok) ::testing::fail(error);
    CHECK(scene.key == "commented");
}

TEST(config_round_trips_the_gravity_solver) {
    // Without this the asteroid belt would silently fall back to direct
    // summation whenever it was loaded from configs/ rather than from code.
    const Scene* belt = findScene("belt");
    CHECK(belt != nullptr);
    CHECK(belt->settings.solver == GravitySolver::BarnesHut);

    Scene restored;
    std::string error;
    const bool ok = sceneFromJson(sceneToJson(*belt), restored, error);
    if (!ok) ::testing::fail(error);

    CHECK(restored.settings.solver == GravitySolver::BarnesHut);
    CHECK(restored.settings.barnesHutTheta == belt->settings.barnesHutTheta);

    // And a scene that does not name a solver defaults to exact summation.
    Scene minimal;
    const char* text = R"({
      "key": "x", "view": {"metresPerUnit": 1},
      "bodies": [{"name": "a", "massKg": 1, "radiusMetres": 1}]
    })";
    CHECK(sceneFromJson(text, minimal, error));
    CHECK(minimal.settings.solver == GravitySolver::Direct);
}
