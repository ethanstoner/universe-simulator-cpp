#include "sim/SceneConfig.h"

#include <fstream>
#include <sstream>

#include "sim/Json.h"

namespace sim {
namespace {

Json vectorToJson(const Vec3& v) {
    Json array = Json::array();
    array.push(Json(v.x));
    array.push(Json(v.y));
    array.push(Json(v.z));
    return array;
}

Vec3 vectorFromJson(const Json& json, const Vec3& fallback = Vec3(0.0)) {
    if (!json.isArray() || json.size() < 3) return fallback;
    return Vec3(json[0].asNumber(fallback.x), json[1].asNumber(fallback.y),
                json[2].asNumber(fallback.z));
}

Json colorToJson(const glm::vec3& c) {
    Json array = Json::array();
    array.push(Json(static_cast<double>(c.r)));
    array.push(Json(static_cast<double>(c.g)));
    array.push(Json(static_cast<double>(c.b)));
    return array;
}

glm::vec3 colorFromJson(const Json& json, const glm::vec3& fallback) {
    if (!json.isArray() || json.size() < 3) return fallback;
    return glm::vec3(static_cast<float>(json[0].asNumber(fallback.r)),
                     static_cast<float>(json[1].asNumber(fallback.g)),
                     static_cast<float>(json[2].asNumber(fallback.b)));
}

const char* integratorKey(IntegratorType type) {
    switch (type) {
        case IntegratorType::ExplicitEuler: return "explicit-euler";
        case IntegratorType::SymplecticEuler: return "symplectic-euler";
        case IntegratorType::VelocityVerlet: return "velocity-verlet";
        case IntegratorType::RungeKutta4: return "rk4";
    }
    return "velocity-verlet";
}

IntegratorType integratorFromKey(const std::string& key) {
    if (key == "explicit-euler") return IntegratorType::ExplicitEuler;
    if (key == "symplectic-euler") return IntegratorType::SymplecticEuler;
    if (key == "rk4") return IntegratorType::RungeKutta4;
    return IntegratorType::VelocityVerlet;
}

const char* stabilizationKey(Stabilization mode) {
    switch (mode) {
        case Stabilization::None: return "none";
        case Stabilization::Softening: return "softening";
        case Stabilization::MinDistance: return "min-distance";
    }
    return "softening";
}

Stabilization stabilizationFromKey(const std::string& key) {
    if (key == "none") return Stabilization::None;
    if (key == "min-distance") return Stabilization::MinDistance;
    return Stabilization::Softening;
}

const char* collisionKey(CollisionMode mode) {
    switch (mode) {
        case CollisionMode::Ignore: return "ignore";
        case CollisionMode::Elastic: return "elastic";
        case CollisionMode::Merge: return "merge";
    }
    return "ignore";
}

CollisionMode collisionFromKey(const std::string& key) {
    if (key == "elastic") return CollisionMode::Elastic;
    if (key == "merge") return CollisionMode::Merge;
    return CollisionMode::Ignore;
}

}  // namespace

std::string sceneToJson(const Scene& scene) {
    Json root = Json::object();
    root.set("key", Json(scene.key));
    root.set("title", Json(scene.title));
    root.set("description", Json(scene.description));
    root.set("fixedTimeStepSeconds", Json(scene.fixedTimeStep));
    root.set("defaultTimeScale", Json(scene.defaultTimeScale));

    Json physics = Json::object();
    physics.set("gravitationalConstant", Json(scene.settings.gravitationalConstant));
    physics.set("integrator", Json(integratorKey(scene.settings.integrator)));
    physics.set("pairwiseGravity", Json(scene.settings.pairwiseGravityEnabled));
    physics.set("solver", Json(scene.settings.solver == GravitySolver::BarnesHut
                                   ? "barnes-hut"
                                   : "direct"));
    physics.set("barnesHutTheta", Json(scene.settings.barnesHutTheta));
    physics.set("relativisticCorrection", Json(scene.settings.relativisticCorrection));
    physics.set("relativisticStrength", Json(scene.settings.relativisticStrength));
    physics.set("stabilization", Json(stabilizationKey(scene.settings.stabilization)));
    physics.set("softeningLengthMetres", Json(scene.settings.softeningLength));
    physics.set("minimumDistanceMetres", Json(scene.settings.minimumDistance));
    physics.set("uniformGravity", vectorToJson(scene.settings.uniformGravity));
    physics.set("collisionMode", Json(collisionKey(scene.settings.collisionMode)));
    physics.set("collisionRestitution", Json(scene.settings.collisionRestitution));
    physics.set("collisionRadiusScale", Json(scene.settings.collisionRadiusScale));
    physics.set("trailLength", Json(static_cast<double>(scene.settings.trailLength)));
    physics.set("trailSampleIntervalSeconds", Json(scene.settings.trailSampleInterval));

    Json bounds = Json::object();
    bounds.set("enabled", Json(scene.settings.bounds.enabled));
    bounds.set("min", vectorToJson(scene.settings.bounds.min));
    bounds.set("max", vectorToJson(scene.settings.bounds.max));
    bounds.set("restitution", Json(scene.settings.bounds.restitution));
    bounds.set("friction", Json(scene.settings.bounds.friction));
    physics.set("bounds", bounds);
    root.set("physics", physics);

    Json view = Json::object();
    view.set("metresPerUnit", Json(scene.view.metresPerUnit));
    view.set("largestBodyDrawnRadius", Json(scene.view.largestBodyDrawnRadius));
    view.set("bodyVisualExponent", Json(scene.view.bodyVisualExponent));
    view.set("scaleReferenceBody", Json(scene.view.scaleReferenceBody));
    view.set("trueScale", Json(scene.view.trueScale));
    view.set("minVisualRadius", Json(scene.view.minVisualRadius));
    view.set("cameraDistance", Json(scene.view.cameraDistance));
    view.set("cameraPitchDegrees", Json(scene.view.cameraPitchDegrees));
    view.set("gridEnabled", Json(scene.view.gridEnabled));
    view.set("gridExtent", Json(scene.view.gridExtent));
    view.set("focusBody", Json(scene.view.focusBody));
    view.set("trailReferenceBody", Json(scene.view.trailReferenceBody));
    root.set("view", view);

    Json bodies = Json::array();
    for (const CelestialBody& body : scene.bodies) {
        Json entry = Json::object();
        entry.set("name", Json(body.name));
        entry.set("massKg", Json(body.mass));
        entry.set("radiusMetres", Json(body.radius));
        entry.set("positionMetres", vectorToJson(body.position));
        entry.set("velocityMetresPerSecond", vectorToJson(body.velocity));
        entry.set("color", colorToJson(body.color));
        entry.set("emissive", Json(body.emissive));
        entry.set("fixed", Json(body.fixed));
        entry.set("showTrail", Json(body.showTrail));
        bodies.push(entry);
    }
    root.set("bodies", bodies);

    return root.dump(2);
}

bool sceneFromJson(const std::string& text, Scene& out, std::string& error) {
    Json root;
    if (!Json::parse(text, root, error)) return false;
    if (!root.isObject()) {
        error = "top level value is not an object";
        return false;
    }

    Scene scene;
    scene.key = root["key"].asString();
    if (scene.key.empty()) {
        error = "scene is missing a \"key\"";
        return false;
    }
    scene.title = root["title"].asString(scene.key);
    scene.description = root["description"].asString();
    scene.fixedTimeStep = root["fixedTimeStepSeconds"].asNumber(1.0 / 120.0);
    if (scene.fixedTimeStep <= 0.0) {
        error = "fixedTimeStepSeconds must be positive";
        return false;
    }
    scene.defaultTimeScale = root["defaultTimeScale"].asNumber(1.0);

    const Json& physics = root["physics"];
    SimulationSettings& settings = scene.settings;
    settings.gravitationalConstant =
        physics["gravitationalConstant"].asNumber(settings.gravitationalConstant);
    settings.integrator = integratorFromKey(physics["integrator"].asString("velocity-verlet"));
    settings.pairwiseGravityEnabled = physics["pairwiseGravity"].asBool(true);
    settings.solver = physics["solver"].asString("direct") == "barnes-hut"
                          ? GravitySolver::BarnesHut
                          : GravitySolver::Direct;
    settings.barnesHutTheta = physics["barnesHutTheta"].asNumber(0.5);
    settings.relativisticCorrection = physics["relativisticCorrection"].asBool(false);
    settings.relativisticStrength = physics["relativisticStrength"].asNumber(1.0);
    settings.stabilization = stabilizationFromKey(physics["stabilization"].asString("softening"));
    settings.softeningLength = physics["softeningLengthMetres"].asNumber(settings.softeningLength);
    settings.minimumDistance = physics["minimumDistanceMetres"].asNumber(settings.minimumDistance);
    settings.uniformGravity = vectorFromJson(physics["uniformGravity"]);
    settings.collisionMode = collisionFromKey(physics["collisionMode"].asString("ignore"));
    settings.collisionRestitution =
        physics["collisionRestitution"].asNumber(settings.collisionRestitution);
    settings.collisionRadiusScale =
        physics["collisionRadiusScale"].asNumber(settings.collisionRadiusScale);
    settings.trailLength = static_cast<std::size_t>(
        std::max(0.0, physics["trailLength"].asNumber(900.0)));
    settings.trailSampleInterval = physics["trailSampleIntervalSeconds"].asNumber(0.0);

    const Json& bounds = physics["bounds"];
    settings.bounds.enabled = bounds["enabled"].asBool(false);
    settings.bounds.min = vectorFromJson(bounds["min"], settings.bounds.min);
    settings.bounds.max = vectorFromJson(bounds["max"], settings.bounds.max);
    settings.bounds.restitution = bounds["restitution"].asNumber(settings.bounds.restitution);
    settings.bounds.friction = bounds["friction"].asNumber(settings.bounds.friction);

    const Json& view = root["view"];
    scene.view.metresPerUnit = view["metresPerUnit"].asNumber(1.0);
    if (scene.view.metresPerUnit <= 0.0) {
        error = "view.metresPerUnit must be positive";
        return false;
    }
    scene.view.largestBodyDrawnRadius = view["largestBodyDrawnRadius"].asNumber(1.2);
    scene.view.bodyVisualExponent = view["bodyVisualExponent"].asNumber(0.35);
    scene.view.scaleReferenceBody = view["scaleReferenceBody"].asString();
    scene.view.trueScale = view["trueScale"].asBool(false);
    scene.view.minVisualRadius = view["minVisualRadius"].asNumber(0.02);
    scene.view.cameraDistance = view["cameraDistance"].asNumber(40.0);
    scene.view.cameraPitchDegrees = view["cameraPitchDegrees"].asNumber(28.0);
    scene.view.gridEnabled = view["gridEnabled"].asBool(true);
    scene.view.gridExtent = view["gridExtent"].asNumber(60.0);
    scene.view.focusBody = view["focusBody"].asString();
    scene.view.trailReferenceBody = view["trailReferenceBody"].asString();

    const Json& bodies = root["bodies"];
    if (!bodies.isArray() || bodies.size() == 0) {
        error = "scene has no \"bodies\" array";
        return false;
    }
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const Json& entry = bodies[i];
        CelestialBody body;
        body.name = entry["name"].asString("body-" + std::to_string(i));
        body.mass = entry["massKg"].asNumber(0.0);
        body.radius = entry["radiusMetres"].asNumber(0.0);
        if (body.mass < 0.0 || body.radius < 0.0) {
            error = "body \"" + body.name + "\" has a negative mass or radius";
            return false;
        }
        body.position = vectorFromJson(entry["positionMetres"]);
        body.velocity = vectorFromJson(entry["velocityMetresPerSecond"]);
        body.color = colorFromJson(entry["color"], glm::vec3(0.8f));
        body.emissive = entry["emissive"].asBool(false);
        body.fixed = entry["fixed"].asBool(false);
        body.showTrail = entry["showTrail"].asBool(true);
        scene.bodies.push_back(std::move(body));
    }

    out = std::move(scene);
    return true;
}

bool saveSceneFile(const Scene& scene, const std::string& path, std::string& error) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        error = "cannot open " + path + " for writing";
        return false;
    }
    file << sceneToJson(scene) << "\n";
    if (!file) {
        error = "write to " + path + " failed";
        return false;
    }
    return true;
}

bool loadSceneFile(const std::string& path, Scene& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "cannot open " + path;
        return false;
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    if (!sceneFromJson(stream.str(), out, error)) {
        error = path + ": " + error;
        return false;
    }
    return true;
}

}  // namespace sim
