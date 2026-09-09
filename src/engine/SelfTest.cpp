#include "engine/SelfTest.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "engine/Application.h"
#include "sim/Diagnostics.h"
#include "sim/SceneConfig.h"
#include "sim/SceneLibrary.h"
#include "sim/ViewMath.h"

namespace engine {
namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool condition, const std::string& what) {
    ++g_checks;
    if (condition) return;
    ++g_failures;
    std::printf("  [FAIL] %s\n", what.c_str());
}

// Every invariant the application is supposed to maintain, asserted after each
// operation. These are exactly the things that broke in practice: dangling
// selections after a delete or a merge, and a trail frame pointing at a body
// that no longer exists.
void checkInvariants(Application& app, const std::string& context) {
    const sim::GravitySystem& system = app.system();
    const AppState& state = app.state();

    if (!system.bodies().empty()) {
        check(system.find(state.selected) != nullptr,
              context + ": selected body must exist");
        check(system.find(state.followed) != nullptr,
              context + ": followed body must exist");
    }
    if (system.settings().trailReference != sim::kInvalidBodyId) {
        check(system.find(system.settings().trailReference) != nullptr,
              context + ": trail reference must exist");
    }

    for (const sim::CelestialBody& body : system.bodies()) {
        check(std::isfinite(body.position.x) && std::isfinite(body.position.y) &&
                  std::isfinite(body.position.z),
              context + ": " + body.name + " position finite");
        check(std::isfinite(body.velocity.x) && std::isfinite(body.speed()),
              context + ": " + body.name + " velocity finite");
        check(body.mass >= 0.0, context + ": " + body.name + " mass non-negative");
        check(body.radius >= 0.0, context + ": " + body.name + " radius non-negative");
    }

    const render::RenderSettings& render = app.renderSettings();
    check(render.metresPerUnit > 0.0, context + ": metresPerUnit positive");
    check(std::isfinite(render.bodyVisualGain) && render.bodyVisualGain > 0.0,
          context + ": body visual gain positive and finite");
    check(app.timeControl().fixedDt > 0.0, context + ": fixed timestep positive");

    const sim::SystemDiagnostics diagnostics = sim::computeDiagnostics(system);
    check(std::isfinite(diagnostics.totalEnergy), context + ": total energy finite");
}

void step(Application& app, int count) {
    for (int i = 0; i < count; ++i) app.system().step(app.timeControl().fixedDt);
}

}  // namespace

int runSelfTest(Application& app) {
    g_failures = 0;
    g_checks = 0;

    const std::vector<std::string> keys = sim::sceneKeys();
    std::printf("[selftest] %zu scenes\n", keys.size());

    for (const std::string& key : keys) {
        app.loadScene(key);
        checkInvariants(app, key + "/load");
        check(app.state().currentSceneKey == key, key + ": scene key recorded");
        check(!app.system().bodies().empty(), key + ": scene has bodies");

        step(app, 200);
        checkInvariants(app, key + "/step");

        // --- the reset buttons --------------------------------------------
        app.resetRenderDefaults();
        checkInvariants(app, key + "/reset-render");
        app.resetSimulationDefaults();
        checkInvariants(app, key + "/reset-sim");
        app.resetCameraDefaults();
        checkInvariants(app, key + "/reset-camera");
        app.resetAllDefaults();
        checkInvariants(app, key + "/reset-all");

        // --- spawn, focus, duplicate, delete -------------------------------
        const std::size_t before = app.system().size();
        const sim::BodyId spawned = app.spawnFromCamera();
        check(spawned != sim::kInvalidBodyId, key + ": spawn returns a valid id");
        check(app.system().size() == before + 1, key + ": spawn increases body count");
        checkInvariants(app, key + "/spawn");

        // A spawned body must actually take part in gravity, not merely exist.
        step(app, 50);
        if (const sim::CelestialBody* body = app.system().find(spawned)) {
            const bool moved = glm::length(body->velocity) > 0.0 ||
                               glm::length(body->acceleration) > 0.0;
            check(moved || app.system().size() < 2,
                  key + ": spawned body participates in the simulation");
        }

        app.focusOn(spawned);
        checkInvariants(app, key + "/focus");
        check(app.state().followed == spawned, key + ": focus follows the body");

        check(app.deleteBody(spawned), key + ": delete removes the spawned body");
        check(app.system().size() == before, key + ": delete restores body count");
        checkInvariants(app, key + "/delete");

        // --- deleting the trail reference ----------------------------------
        if (app.system().settings().trailReference != sim::kInvalidBodyId) {
            const sim::BodyId reference = app.system().settings().trailReference;
            app.deleteBody(reference);
            check(app.system().settings().trailReference == sim::kInvalidBodyId,
                  key + ": deleting the trail reference clears it");
            checkInvariants(app, key + "/delete-trail-reference");
            app.loadScene(key);
        }

        // --- physics options reachable from the Simulation panel ------------
        for (int i = 0; i < sim::integratorCount(); ++i) {
            app.system().settings().integrator = sim::integratorFromIndex(i);
            app.system().invalidate();
            step(app, 30);
            checkInvariants(app, key + "/integrator-" + std::to_string(i));
        }
        for (int mode = 0; mode < 3; ++mode) {
            app.system().settings().stabilization = static_cast<sim::Stabilization>(mode);
            step(app, 20);
            checkInvariants(app, key + "/stabilisation-" + std::to_string(mode));
        }
        app.resetSimulationDefaults();

        // --- emptying the scene entirely ------------------------------------
        // The Delete button can be pressed until nothing is left. Everything
        // downstream has to cope with zero bodies rather than dividing by a
        // zero total mass or indexing an empty list.
        while (!app.system().bodies().empty()) {
            app.deleteBody(app.system().bodies().front().id);
        }
        check(app.system().size() == 0, key + ": every body can be deleted");
        step(app, 10);
        checkInvariants(app, key + "/emptied");
        app.resetRenderDefaults();
        app.resetCameraDefaults();
        app.focusOn(sim::kInvalidBodyId);
        checkInvariants(app, key + "/empty-reset");

        // Recovering from empty by reloading must work.
        app.loadScene(key);
        check(!app.system().bodies().empty(), key + ": scene reloads after emptying");
        checkInvariants(app, key + "/reload");
    }

    // --- picking against every scene, which is what click-to-select uses ----
    app.loadScene("inner");
    {
        const sim::ViewScale scale = render::Renderer::toViewScale(app.renderSettings());
        int occluded = 0;

        for (const sim::CelestialBody& body : app.system().bodies()) {
            const sim::Vec3 centre = body.position / scale.metresPerUnit;
            const sim::Vec3 camera = centre + sim::Vec3(0.0, 0.0, 6.0);
            const sim::Vec3 direction = sim::safeNormalize(centre - camera);
            const sim::BodyId hit = sim::pickBody(app.system(), camera, direction, scale);

            // Aiming straight at a body must always hit *something*.
            check(hit != sim::kInvalidBodyId, "aiming at " + body.name + " hits a body");
            if (hit == body.id) continue;

            // Not a picking bug when it misses: at 1 AU = 10 units the Moon sits
            // 0.026 units from Earth, well inside Earth's ~0.46 unit drawn
            // sphere, so the ray legitimately hits Earth first. Require that
            // the body it DID hit actually encloses the target.
            const sim::CelestialBody* blocker = app.system().find(hit);
            check(blocker != nullptr, "picked body exists");
            if (!blocker) continue;
            const sim::Vec3 blockerCentre = blocker->position / scale.metresPerUnit;
            const double separation = glm::length(blockerCentre - centre);
            const double blockerRadius =
                std::max(sim::visualRadius(*blocker, scale), 0.05);
            check(separation <= blockerRadius,
                  body.name + " is only unpickable because " + blocker->name +
                      " encloses it");
            ++occluded;
        }
        // Reported rather than hidden: it is a real usability limit, and the
        // body list and Tab cycling exist to reach those bodies.
        if (occluded > 0) {
            std::printf("[selftest] %d body(s) unpickable at this zoom "
                        "(enclosed by a larger body); selectable via the list\n",
                        occluded);
        }
    }

    // --- configuration round trip through the registry ----------------------
    for (const sim::Scene& scene : sim::builtinScenes()) {
        sim::Scene restored;
        std::string error;
        check(sim::sceneFromJson(sim::sceneToJson(scene), restored, error),
              "scene " + scene.key + " round-trips through JSON: " + error);
    }

    std::printf("[selftest] %d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}

}  // namespace engine
