#include "engine/Application.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <filesystem>

#include "engine/AssetPaths.h"
#include "engine/Screenshot.h"
#include "engine/SelfTest.h"
#include "sim/Constants.h"
#include "sim/OrbitMath.h"
#include "sim/SceneConfig.h"
#include "ui/DebugUI.h"

namespace engine {

int loadSceneConfigs(const std::string& directory) {
    std::error_code code;
    const std::filesystem::path root(resolveAsset(directory));
    if (!std::filesystem::is_directory(root, code)) return 0;

    // Every *.json in configs/ is registered, replacing the compiled-in preset
    // of the same key. Editing a mass or an orbital radius therefore needs no
    // rebuild. A malformed file is reported and skipped rather than being
    // allowed to take the application down.
    int loaded = 0;
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(root, code)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    for (const std::filesystem::path& file : files) {
        sim::Scene scene;
        std::string error;
        if (sim::loadSceneFile(file.string(), scene, error)) {
            sim::registerScene(std::move(scene));
            ++loaded;
        } else {
            std::fprintf(stderr, "[config] skipped: %s\n", error.c_str());
        }
    }
    if (loaded > 0) std::printf("[config] loaded %d scene(s) from %s\n", loaded,
                                root.string().c_str());
    return loaded;
}

int exportSceneConfigs(const std::string& directory) {
    std::error_code code;
    std::filesystem::create_directories(directory, code);
    int written = 0;
    for (const sim::Scene& scene : sim::builtinScenes()) {
        const std::string path = (std::filesystem::path(directory) /
                                  (scene.key + ".json")).string();
        std::string error;
        if (sim::saveSceneFile(scene, path, error)) {
            std::printf("[config] wrote %s\n", path.c_str());
            ++written;
        } else {
            std::fprintf(stderr, "[config] %s\n", error.c_str());
        }
    }
    return written;
}

void AppOptions::printUsage() {
    std::printf(
        "universe-sim -- interactive Newtonian gravity simulator\n"
        "\n"
        "  --scene NAME             scene preset to load (default solar-system)\n"
        "  --list-scenes            print the available presets and exit\n"
        "  --width N --height N     framebuffer size (default 1600x900)\n"
        "  --screenshot PATH        render offscreen, write a PNG, exit\n"
        "  --frame N                frame to capture on (default 30)\n"
        "  --warmup SECONDS         simulated seconds to advance before capture\n"
        "  --focus NAME             frame this body at startup\n"
        "  --distance UNITS         camera distance override\n"
        "  --pitch DEG --yaw DEG    camera angle override\n"
        "  --no-ui                  hide the ImGui panels\n"
        "  --export-configs DIR     write every scene to DIR as JSON and exit\n"
        "  --no-configs             ignore configs/ and use the built-in presets\n"
        "  --no-post                disable HDR post-processing and bloom\n"
        "  --no-stars               hide the background starfield\n"
        "  --selftest               exercise every UI-reachable path and exit\n"
        "  --sequence DIR           write a deterministic PNG frame sequence and exit\n"
        "  --sequence-frames N      how many frames to write\n"
        "  --sequence-step SECONDS  simulated seconds advanced per frame\n"
        "  --sequence-orbit DEG     camera yaw swept across the whole sequence\n"
        "  --spawn PRESET           spawn a body preset after the warm-up (repeatable)\n"
        "  --spawn-distance UNITS   how far ahead of the camera to spawn it\n"
        "  --spawn-at-rest          spawn with no automatic circular-orbit velocity\n"
        "  --settle SECONDS         simulated seconds to advance AFTER spawning\n"
        "  --list-spawn-presets     print the body presets and exit\n"
        "  --hidden                 do not show the window (implied by screenshot)\n"
        "  --no-vsync               uncap the frame rate\n"
        "  --help                   this message\n");
}

AppOptions AppOptions::parse(int argc, char** argv) {
    AppOptions options;
    auto valueFor = [&](int& i) -> const char* {
        if (i + 1 >= argc) {
            std::fprintf(stderr, "missing value for %s\n", argv[i]);
            std::exit(2);
        }
        return argv[++i];
    };

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (!std::strcmp(arg, "--width")) options.width = std::atoi(valueFor(i));
        else if (!std::strcmp(arg, "--height")) options.height = std::atoi(valueFor(i));
        else if (!std::strcmp(arg, "--scene")) options.scene = valueFor(i);
        else if (!std::strcmp(arg, "--list-scenes")) options.listScenes = true;
        else if (!std::strcmp(arg, "--screenshot")) options.screenshotPath = valueFor(i);
        else if (!std::strcmp(arg, "--frame")) options.screenshotFrame = std::atoi(valueFor(i));
        else if (!std::strcmp(arg, "--warmup")) options.warmupSimSeconds = std::atof(valueFor(i));
        else if (!std::strcmp(arg, "--focus")) options.focus = valueFor(i);
        else if (!std::strcmp(arg, "--distance")) options.cameraDistance = std::atof(valueFor(i));
        else if (!std::strcmp(arg, "--pitch")) options.cameraPitch = std::atof(valueFor(i));
        else if (!std::strcmp(arg, "--yaw")) options.cameraYaw = std::atof(valueFor(i));
        else if (!std::strcmp(arg, "--no-ui")) options.noUi = true;
        else if (!std::strcmp(arg, "--export-configs")) options.exportConfigs = valueFor(i);
        else if (!std::strcmp(arg, "--no-configs")) options.noConfigs = true;
        else if (!std::strcmp(arg, "--no-post")) options.noPost = true;
        else if (!std::strcmp(arg, "--no-stars")) options.noStars = true;
        else if (!std::strcmp(arg, "--selftest")) {
            options.selfTest = true;
            options.hidden = true;
        }
        else if (!std::strcmp(arg, "--sequence")) {
            options.sequenceDir = valueFor(i);
            options.hidden = true;
            options.vsync = false;
        }
        else if (!std::strcmp(arg, "--sequence-frames")) {
            options.sequenceFrames = std::atoi(valueFor(i));
        }
        else if (!std::strcmp(arg, "--sequence-step")) {
            options.sequenceStepSeconds = std::atof(valueFor(i));
        }
        else if (!std::strcmp(arg, "--sequence-orbit")) {
            options.sequenceOrbitDegrees = std::atof(valueFor(i));
        }
        else if (!std::strcmp(arg, "--spawn")) options.spawnPresets.emplace_back(valueFor(i));
        else if (!std::strcmp(arg, "--spawn-distance")) {
            options.spawnDistance = std::atof(valueFor(i));
        } else if (!std::strcmp(arg, "--spawn-at-rest")) options.spawnAtRest = true;
        else if (!std::strcmp(arg, "--settle")) {
            options.settleSimSeconds = std::atof(valueFor(i));
        }
        else if (!std::strcmp(arg, "--list-spawn-presets")) {
            for (const sim::BodyPreset& preset : sim::bodyPresets()) {
                std::printf("%-24s %.4g kg  r = %.4g m\n", preset.name.c_str(),
                            preset.mass, preset.radius);
            }
            std::exit(0);
        }
        else if (!std::strcmp(arg, "--hidden")) options.hidden = true;
        else if (!std::strcmp(arg, "--no-vsync")) options.vsync = false;
        else if (!std::strcmp(arg, "--help") || !std::strcmp(arg, "-h")) {
            printUsage();
            std::exit(0);
        } else {
            std::fprintf(stderr, "unknown option: %s\n", arg);
            printUsage();
            std::exit(2);
        }
    }

    if (!options.screenshotPath.empty()) {
        options.hidden = true;
        options.vsync = false;  // do not wait on the compositor when headless
    }
    return options;
}

Application::Application(const AppOptions& options) : options_(options) {
    // Configs are registered before any scene is resolved, so a JSON file can
    // replace a built-in preset that the --scene argument then names.
    if (!options_.noConfigs) loadSceneConfigs("configs");

    WindowConfig config;
    config.width = options.width;
    config.height = options.height;
    config.title = "universe-sim";
    config.visible = !options.hidden;
    config.vsync = options.vsync;
    window_ = std::make_unique<Window>(config);
    input_.attach(window_->handle());

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_LINE_SMOOTH);

    if (options_.noPost) renderSettings_.postProcessEnabled = false;
    if (options_.noStars) renderSettings_.showStarfield = false;

    renderReady_ = renderer_.initialize(renderSettings_);
    if (!renderReady_) {
        std::fprintf(stderr, "[render] shader setup failed; the scene will not draw\n");
    }

    if (!options_.noUi) {
        ui_ = std::make_unique<ui::DebugUI>();
        ui_->initialize(window_->handle());
    }

    loadScene(options_.scene);
}

Application::~Application() {
    if (ui_) ui_->shutdown();
}

void Application::setStatus(const std::string& message) {
    state_.statusMessage = message;
    state_.statusMessageAge = 0.0;
    std::printf("[app] %s\n", message.c_str());
}

void Application::applySceneView(const sim::Scene& scene) {
    const sim::SceneView& view = scene.view;
    renderSettings_.metresPerUnit = view.metresPerUnit;
    renderSettings_.trueScale = view.trueScale;
    renderSettings_.minVisualRadius = view.minVisualRadius;
    renderSettings_.bodyVisualExponent = view.bodyVisualExponent;
    // The power-law gain is solved once per scene from its largest body, so
    // every preset lands at a legible size without a hand-tuned multiplier.
    renderSettings_.bodyVisualGain = render::Renderer::gainForLargestRadius(
        system_, view.metresPerUnit, renderSettings_.bodyVisualExponent,
        view.largestBodyDrawnRadius, view.scaleReferenceBody);
    renderSettings_.showGrid = view.gridEnabled;
    renderSettings_.gridExtent = view.gridExtent;
    // The well has to be deep enough to read as a funnel but shallow enough
    // that bodies do not appear to float high above their own dimple. Tying it
    // to the viewing distance as well as the patch size keeps that balance when
    // a scene is framed from far back.
    renderSettings_.gridMaxDepth =
        std::max(std::min(view.gridExtent * 0.16, view.cameraDistance * 0.14), 0.5);
    renderSettings_.showBoundsBox = scene.settings.bounds.enabled;

    time_.fixedDt = scene.fixedTimeStep;
    time_.timeScale = scene.defaultTimeScale;
    time_.reset();

    camera_.yaw = options_.cameraYaw < 1e8 ? static_cast<float>(options_.cameraYaw) : -90.0f;
    camera_.pitch = options_.cameraPitch < 1e8
                        ? static_cast<float>(options_.cameraPitch)
                        : -static_cast<float>(view.cameraPitchDegrees);
    camera_.moveSpeed = static_cast<float>(view.cameraDistance * 0.4);

    double distance = options_.cameraDistance > 0.0 ? options_.cameraDistance
                                                    : view.cameraDistance;
    camera_.setMode(CameraMode::Orbit);

    // Trail frames are named in the scene but keyed by id at runtime, so
    // resolve the name now that applyScene has assigned ids.
    system_.settings().trailReference = sim::kInvalidBodyId;
    if (!view.trailReferenceBody.empty()) {
        for (const sim::CelestialBody& body : system_.bodies()) {
            if (body.name == view.trailReferenceBody) {
                system_.settings().trailReference = body.id;
                break;
            }
        }
    }

    const std::string focusName = !options_.focus.empty() ? options_.focus : view.focusBody;
    state_.followBarycentre = (focusName == "barycentre" || focusName == "barycenter");
    state_.followed = sim::kInvalidBodyId;
    for (const sim::CelestialBody& body : system_.bodies()) {
        if (body.name == focusName) {
            state_.followed = body.id;
            break;
        }
    }
    if (state_.followed == sim::kInvalidBodyId && !system_.bodies().empty()) {
        state_.followed = system_.bodies().front().id;
    }
    state_.selected = state_.followed;

    glm::dvec3 target(0.0);
    if (state_.followBarycentre) {
        const sim::SystemDiagnostics d = sim::computeDiagnostics(system_);
        if (d.totalMass > 0.0) target = glm::dvec3(d.centreOfMass) / view.metresPerUnit;
    } else if (const sim::CelestialBody* body = system_.find(state_.followed)) {
        target = glm::dvec3(body->position) / renderSettings_.metresPerUnit;
    }
    camera_.frame(target, distance);
}

void Application::loadScene(const std::string& key) {
    const sim::Scene* scene = sim::findScene(key);
    if (!scene) {
        std::fprintf(stderr, "unknown scene '%s'; using the first preset\n", key.c_str());
        scene = &sim::builtinScenes().front();
    }

    sim::applyScene(*scene, system_);
    state_.currentSceneKey = scene->key;
    mergeCount_ = 0;

    // The scene's own view hints drive the render scale, camera and step size.
    applySceneView(*scene);

    diagnostics_ = sim::computeDiagnostics(system_);
    energyTracker_.reset(diagnostics_);

    // The spawn template starts as something sensible for this scene's scale.
    state_.spawnTemplate = sim::makeBody("New body", sim::constants::kEarthMass,
                                         sim::constants::kEarthRadius, sim::Vec3(0.0),
                                         sim::Vec3(0.0), glm::vec3(0.6f, 0.9f, 0.7f));
    state_.spawnDistance = scene->view.cameraDistance * 0.35;

    setStatus("Loaded scene: " + scene->title);
}

void Application::resetScene() { loadScene(state_.currentSceneKey); }

void Application::repairDanglingReferences() {
    const bool empty = system_.bodies().empty();
    const sim::BodyId fallback = empty ? sim::kInvalidBodyId : system_.bodies().front().id;

    if (!system_.find(state_.selected)) state_.selected = fallback;
    if (!system_.find(state_.followed)) state_.followed = fallback;
    // A trail frame pointing at a deleted body would silently reinterpret every
    // stored sample as being in the inertial frame, which makes trails jump.
    if (system_.settings().trailReference != sim::kInvalidBodyId &&
        !system_.find(system_.settings().trailReference)) {
        system_.settings().trailReference = sim::kInvalidBodyId;
        system_.clearTrails();
    }
}

bool Application::deleteBody(sim::BodyId id) {
    const sim::CelestialBody* body = system_.find(id);
    if (!body) return false;
    const std::string name = body->name;  // copied before the vector is mutated

    if (!system_.remove(id)) return false;
    repairDanglingReferences();

    // Removing mass changes the system's real total energy, so the drift
    // reference has to move with it or the change is reported as drift.
    diagnostics_ = sim::computeDiagnostics(system_);
    energyTracker_.reset(diagnostics_);

    setStatus("Deleted " + name);
    return true;
}

void Application::resetRenderDefaults() {
    // Start from a fresh RenderSettings so every look-and-feel value returns to
    // its compiled-in default, then re-apply the current scene's view hints on
    // top so scale and framing stay correct for what is loaded.
    const render::RenderSettings defaults;
    const double metresPerUnit = renderSettings_.metresPerUnit;
    renderSettings_ = defaults;
    renderSettings_.metresPerUnit = metresPerUnit;

    if (const sim::Scene* scene = sim::findScene(state_.currentSceneKey)) {
        const sim::SceneView& view = scene->view;
        renderSettings_.metresPerUnit = view.metresPerUnit;
        renderSettings_.trueScale = view.trueScale;
        renderSettings_.minVisualRadius = view.minVisualRadius;
        renderSettings_.bodyVisualExponent = view.bodyVisualExponent;
        renderSettings_.bodyVisualGain = render::Renderer::gainForLargestRadius(
            system_, view.metresPerUnit, view.bodyVisualExponent,
            view.largestBodyDrawnRadius, view.scaleReferenceBody);
        renderSettings_.showGrid = view.gridEnabled;
        renderSettings_.gridExtent = view.gridExtent;
        renderSettings_.gridMaxDepth =
            std::max(std::min(view.gridExtent * 0.16, view.cameraDistance * 0.14), 0.5);
        renderSettings_.showBoundsBox = scene->settings.bounds.enabled;
    }
    // Command line overrides still win, so --no-post stays off after a reset.
    if (options_.noPost) renderSettings_.postProcessEnabled = false;
    if (options_.noStars) renderSettings_.showStarfield = false;

    setStatus("Rendering settings reset to defaults");
}

void Application::resetSimulationDefaults() {
    const sim::Scene* scene = sim::findScene(state_.currentSceneKey);
    if (!scene) return;

    // Physics *settings* only. Body states are left alone, so this is not a
    // scene restart: it undoes integrator/softening/collision experiments
    // without throwing away the run.
    const sim::BodyId trailReference = system_.settings().trailReference;
    system_.settings() = scene->settings;
    system_.settings().trailReference = trailReference;
    system_.invalidate();

    time_.fixedDt = scene->fixedTimeStep;
    time_.timeScale = scene->defaultTimeScale;
    time_.maxStepsPerFrame = engine::TimeControl{}.maxStepsPerFrame;
    time_.paused = false;
    time_.reset();

    diagnostics_ = sim::computeDiagnostics(system_);
    energyTracker_.reset(diagnostics_);
    setStatus("Simulation settings reset to scene defaults");
}

void Application::resetCameraDefaults() {
    const sim::Scene* scene = sim::findScene(state_.currentSceneKey);
    if (!scene) return;
    const sim::SceneView& view = scene->view;

    camera_ = Camera{};  // fov, sensitivity, near/far all back to defaults
    camera_.yaw = -90.0f;
    camera_.pitch = -static_cast<float>(view.cameraPitchDegrees);
    camera_.moveSpeed = static_cast<float>(view.cameraDistance * 0.4);
    camera_.setMode(CameraMode::Orbit);

    glm::dvec3 target(0.0);
    if (const sim::CelestialBody* body = system_.find(state_.followed)) {
        target = glm::dvec3(body->position) / renderSettings_.metresPerUnit;
    }
    camera_.frame(target, view.cameraDistance);
    setStatus("Camera reset");
}

void Application::resetAllDefaults() {
    resetRenderDefaults();
    resetSimulationDefaults();
    resetCameraDefaults();
    setStatus("All settings reset to defaults");
}

sim::BodyId Application::spawnBody(const sim::CelestialBody& body) {
    sim::CelestialBody copy = body;
    copy.id = sim::kInvalidBodyId;
    copy.trail.clear();
    const sim::BodyId id = system_.add(copy);
    // The reference energy has to move with the system: adding mass changes the
    // total energy, and reporting that jump as "drift" would be misleading.
    diagnostics_ = sim::computeDiagnostics(system_);
    energyTracker_.reset(diagnostics_);
    return id;
}

sim::BodyId Application::spawnFromCamera() {
    // Place the body along the camera's forward axis at the configured
    // distance, then convert that world-unit point back into metres.
    const glm::dvec3 point =
        camera_.position() + glm::dvec3(camera_.forward()) * state_.spawnDistance;
    sim::CelestialBody body = state_.spawnTemplate;
    body.position = sim::Vec3(point * renderSettings_.metresPerUnit);

    if (state_.spawnOrbitAuto) {
        // Give it the circular orbit velocity for the most massive body in the
        // scene, so a spawned planet does not simply fall straight in.
        const sim::CelestialBody* primary = nullptr;
        for (const sim::CelestialBody& candidate : system_.bodies()) {
            if (!primary || candidate.mass > primary->mass) primary = &candidate;
        }
        if (primary && primary->mass > 0.0) {
            body.velocity =
                primary->velocity +
                sim::circularOrbitVelocity(primary->position, primary->mass, body.position,
                                           sim::Vec3(0.0, 1.0, 0.0),
                                           system_.settings().gravitationalConstant);
        }
    }

    const sim::BodyId id = spawnBody(body);
    state_.selected = id;
    setStatus("Spawned " + body.name);
    return id;
}

void Application::focusOn(sim::BodyId id) {
    const sim::CelestialBody* body = system_.find(id);
    if (!body) return;
    state_.followBarycentre = false;
    state_.followed = id;
    state_.selected = id;
    camera_.setMode(CameraMode::Orbit);

    // Pull in close enough that the body fills a reasonable part of the view,
    // but never inside its own drawn radius.
    const double radius = render::Renderer::visualRadius(*body, renderSettings_);
    const double distance = std::max(radius * 6.0, renderSettings_.gridExtent * 0.06);
    camera_.frame(glm::dvec3(body->position) / renderSettings_.metresPerUnit, distance);
    setStatus("Focused on " + body->name);
}

void Application::updateFollowCamera() {
    if (camera_.mode() != CameraMode::Orbit) return;

    if (state_.followBarycentre) {
        if (diagnostics_.totalMass > 0.0) {
            camera_.setOrbitTarget(glm::dvec3(diagnostics_.centreOfMass) /
                                   renderSettings_.metresPerUnit);
        }
        return;
    }
    const sim::CelestialBody* body = system_.find(state_.followed);
    if (!body) return;
    camera_.setOrbitTarget(glm::dvec3(body->position) / renderSettings_.metresPerUnit);
}

int Application::runSequence() {
    std::error_code code;
    std::filesystem::create_directories(options_.sequenceDir, code);

    const int frames = std::max(options_.sequenceFrames, 1);
    const double stepSeconds = options_.sequenceStepSeconds > 0.0
                                   ? options_.sequenceStepSeconds
                                   : time_.fixedDt;
    const long long stepsPerFrame =
        std::max<long long>(1, static_cast<long long>(stepSeconds / time_.fixedDt));
    const float startYaw = camera_.yaw;

    std::printf("[sequence] %d frames, %.4g simulated seconds each (%lld steps)\n",
                frames, stepSeconds, stepsPerFrame);

    for (int frame = 0; frame < frames; ++frame) {
        // Simulated time is advanced by a fixed amount per frame rather than by
        // the wall clock, so the same command always produces the same footage.
        for (long long i = 0; i < stepsPerFrame; ++i) system_.step(time_.fixedDt);
        diagnostics_ = sim::computeDiagnostics(system_);
        energyTracker_.update(diagnostics_);
        repairDanglingReferences();

        if (options_.sequenceOrbitDegrees != 0.0) {
            const double t = static_cast<double>(frame) / std::max(frames - 1, 1);
            camera_.yaw = startYaw + static_cast<float>(options_.sequenceOrbitDegrees * t);
        }
        updateFollowCamera();
        // Recompute the camera's position from its orbit target. Without this
        // the camera stays wherever it started and the yaw sweep merely pans
        // the view, so a scene whose bodies move drifts out of frame.
        camera_.update(input_, 0.0, /*acceptInput=*/false);

        render();
        if (ui_) {
            ui_->beginFrame();
            ui_->build(*this);
            ui_->endFrame();
        }

        char path[1024];
        std::snprintf(path, sizeof(path), "%s/frame_%05d.png",
                      options_.sequenceDir.c_str(), frame);
        if (!captureFramebufferToPng(window_->framebufferWidth(),
                                     window_->framebufferHeight(), path)) {
            std::fprintf(stderr, "[sequence] failed to write %s\n", path);
            return 1;
        }
        window_->swapBuffers();
        Window::pollEvents();
    }
    std::printf("[sequence] wrote %d frames to %s\n", frames,
                options_.sequenceDir.c_str());
    return 0;
}

int Application::run() {
    if (options_.selfTest) return runSelfTest(*this);

    if (options_.listScenes) {
        for (const sim::Scene& scene : sim::builtinScenes()) {
            std::printf("%-16s %s\n", scene.key.c_str(), scene.title.c_str());
        }
        return 0;
    }

    // Warm-up and scripted spawns apply to sequences too, so a clip can start
    // from a developed system.
    // Headless capture needs deterministic content, so advance by a requested
    // amount of *simulated* time rather than hoping the wall clock cooperates.
    if (options_.warmupSimSeconds > 0.0) {
        const long long steps =
            static_cast<long long>(options_.warmupSimSeconds / time_.fixedDt);
        for (long long i = 0; i < steps; ++i) system_.step(time_.fixedDt);
        diagnostics_ = sim::computeDiagnostics(system_);
        std::printf("[app] warmed up %.4g simulated seconds in %lld steps\n",
                    options_.warmupSimSeconds, steps);
    }

    // Scripted spawns go through spawnFromCamera(), the same path the N key and
    // the Spawn button use, so a screenshot taken afterwards is evidence about
    // the real feature rather than about a test-only shortcut.
    if (!options_.spawnPresets.empty()) {
        if (options_.spawnDistance > 0.0) state_.spawnDistance = options_.spawnDistance;
        if (options_.spawnAtRest) state_.spawnOrbitAuto = false;
        for (const std::string& name : options_.spawnPresets) {
            const sim::BodyPreset* preset = sim::findBodyPreset(name);
            if (!preset) {
                std::fprintf(stderr, "unknown spawn preset '%s'\n", name.c_str());
                continue;
            }
            state_.spawnTemplate.name = preset->name;
            state_.spawnTemplate.mass = preset->mass;
            state_.spawnTemplate.radius = preset->radius;
            state_.spawnTemplate.color = preset->color;
            state_.spawnTemplate.emissive = preset->emissive;

            const sim::BodyId id = spawnFromCamera();
            if (const sim::CelestialBody* body = system_.find(id)) {
                std::printf("[app] spawned %s: m = %.4g kg, r = %.4g m, "
                            "pos = (%.4g, %.4g, %.4g) m, |v| = %.4g m/s, "
                            "%zu bodies now\n",
                            body->name.c_str(), body->mass, body->radius,
                            body->position.x, body->position.y, body->position.z,
                            glm::length(body->velocity), system_.size());
            }
        }
    }

    // Settling time runs after the spawns, so a captured frame shows what the
    // new body actually did to the existing orbits rather than the instant it
    // appeared. Radii are reported before and after so the disruption is a
    // measured number, not an impression from a picture.
    if (options_.settleSimSeconds > 0.0) {
        // Keyed by id, not name: a spawned "Sun" shares its name with the
        // original, and matching on the name reported both twice.
        std::vector<std::pair<sim::BodyId, double>> before;
        for (const sim::CelestialBody& body : system_.bodies()) {
            before.emplace_back(body.id, glm::length(body.position));
        }

        const long long steps =
            static_cast<long long>(options_.settleSimSeconds / time_.fixedDt);
        for (long long i = 0; i < steps; ++i) system_.step(time_.fixedDt);
        diagnostics_ = sim::computeDiagnostics(system_);
        energyTracker_.update(diagnostics_);

        std::printf("[app] settled %.4g simulated seconds in %lld steps\n",
                    options_.settleSimSeconds, steps);
        for (const auto& [id, initialRadius] : before) {
            const sim::CelestialBody* body = system_.find(id);
            if (!body) {
                std::printf("[app]   body %u no longer exists (merged)\n", id);
                continue;
            }
            const double now = glm::length(body->position);
            const double change =
                initialRadius > 0.0 ? (now - initialRadius) / initialRadius * 100.0 : 0.0;
            std::printf("[app]   #%-3u %-14s r %.4g -> %.4g m (%+.2f%%)\n", id,
                        body->name.c_str(), initialRadius, now, change);
        }
    }

    // A frame sequence replaces the interactive loop entirely.
    if (!options_.sequenceDir.empty()) return runSequence();

    while (!window_->shouldClose()) {
        clock_.tick();
        input_.newFrame();
        if (ui_) ui_->beginFrame();

        processInput();
        advanceSimulation(clock_.deltaSeconds());
        updateFollowCamera();
        render();

        if (ui_) {
            ui_->build(*this);
            ui_->endFrame();
        }

        if (!options_.screenshotPath.empty() &&
            static_cast<long long>(clock_.frameCount()) >= options_.screenshotFrame) {
            captureFramebufferToPng(window_->framebufferWidth(),
                                    window_->framebufferHeight(),
                                    options_.screenshotPath);
            window_->requestClose();
        }

        window_->swapBuffers();
        Window::pollEvents();
    }
    return 0;
}

void Application::processInput() {
    const bool uiWantsKeyboard = ui_ && ui_->wantsKeyboard();
    const bool uiWantsMouse = ui_ && ui_->wantsMouse();

    if (input_.keyPressed(GLFW_KEY_ESCAPE)) {
        if (input_.mouseCaptured()) input_.setMouseCaptured(false);
        else window_->requestClose();
    }
    if (input_.keyPressed(GLFW_KEY_F12)) {
        captureFramebufferToPng(window_->framebufferWidth(),
                                window_->framebufferHeight(), "screenshot.png");
    }
    if (input_.keyPressed(GLFW_KEY_F5)) {
        renderer_.reloadShaders();
        setStatus("Shaders reloaded");
    }

    // Right mouse drag captures the cursor for free look; this is what lets the
    // ImGui panels stay clickable the rest of the time.
    if (!uiWantsMouse && input_.mousePressed(GLFW_MOUSE_BUTTON_RIGHT)) {
        input_.setMouseCaptured(true);
    }
    if (input_.mouseReleased(GLFW_MOUSE_BUTTON_RIGHT)) {
        input_.setMouseCaptured(false);
    }

    // Left click selects the body under the cursor.
    if (!uiWantsMouse && !input_.mouseCaptured() &&
        input_.mousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
        glm::vec3 direction;
        const glm::vec2 cursor = input_.mousePosition();
        camera_.screenRay(cursor.x, cursor.y, window_->framebufferWidth(),
                          window_->framebufferHeight(), direction);
        const sim::BodyId hit = render::Renderer::pick(system_, camera_.position(),
                                                       direction, renderSettings_);
        if (hit != sim::kInvalidBodyId) {
            state_.selected = hit;
            const sim::CelestialBody* body = system_.find(hit);
            if (body) setStatus("Selected " + body->name);
        }
    }

    if (!uiWantsKeyboard) {
        if (input_.keyPressed(GLFW_KEY_P)) time_.paused = !time_.paused;
        if (input_.keyPressed(GLFW_KEY_PERIOD)) time_.singleStepRequested = true;
        if (input_.keyPressed(GLFW_KEY_R)) resetScene();
        if (input_.keyPressed(GLFW_KEY_G)) renderSettings_.showGrid = !renderSettings_.showGrid;
        if (input_.keyPressed(GLFW_KEY_T)) renderSettings_.showTrails = !renderSettings_.showTrails;
        if (input_.keyPressed(GLFW_KEY_V)) {
            renderSettings_.showVelocityVectors = !renderSettings_.showVelocityVectors;
        }
        if (input_.keyPressed(GLFW_KEY_B)) {
            renderSettings_.showSchwarzschildRadius = !renderSettings_.showSchwarzschildRadius;
        }
        if (input_.keyPressed(GLFW_KEY_N)) spawnFromCamera();
        if (input_.keyPressed(GLFW_KEY_F)) focusOn(state_.selected);
        if (input_.keyPressed(GLFW_KEY_C)) {
            camera_.setMode(camera_.mode() == CameraMode::Orbit ? CameraMode::Fly
                                                                : CameraMode::Orbit);
            setStatus(camera_.mode() == CameraMode::Orbit ? "Camera: orbit"
                                                          : "Camera: free flight");
        }
        if (input_.keyPressed(GLFW_KEY_LEFT_BRACKET)) {
            time_.timeScale = std::max(time_.timeScale * 0.5, 1e-6);
        }
        if (input_.keyPressed(GLFW_KEY_RIGHT_BRACKET)) {
            time_.timeScale = std::min(time_.timeScale * 2.0, 1e12);
        }
        if (input_.keyPressed(GLFW_KEY_TAB)) {
            // Cycle the selection through the body list.
            const std::vector<sim::CelestialBody>& bodies = system_.bodies();
            if (!bodies.empty()) {
                std::size_t index = 0;
                for (std::size_t i = 0; i < bodies.size(); ++i) {
                    if (bodies[i].id == state_.selected) {
                        index = (i + 1) % bodies.size();
                        break;
                    }
                }
                state_.selected = bodies[index].id;
            }
        }
    }

    camera_.update(input_, clock_.deltaSeconds(), !uiWantsKeyboard || input_.mouseCaptured());
}

void Application::advanceSimulation(double realDelta) {
    const int steps = time_.stepsForFrame(realDelta);
    for (int i = 0; i < steps; ++i) {
        const sim::StepReport report = system_.step(time_.fixedDt);
        if (report.merges > 0) {
            mergeCount_ += report.merges;
            // A merge removes a body, so the selection, the camera focus and
            // the trail reference frame may all now name something that no
            // longer exists.
            repairDanglingReferences();
        }
    }

    if (steps > 0 || diagnostics_.totalMass == 0.0) {
        diagnostics_ = sim::computeDiagnostics(system_);
        energyTracker_.update(diagnostics_);
    }

    state_.statusMessageAge += realDelta;
}

void Application::render() {
    // 4x MSAA matches the window hint, so the offscreen HDR target has the same
    // edge quality the default framebuffer would have had.
    renderer_.beginFrame(window_->framebufferWidth(), window_->framebufferHeight(), 4,
                         renderSettings_);
    if (renderReady_) {
        renderer_.drawScene(system_, camera_, renderSettings_, window_->aspect(),
                            state_.selected);
    }
    // Resolves and composites to the window. ImGui draws after this, so the
    // panels are never tone mapped or bloomed.
    renderer_.endFrame(renderSettings_);
}

}  // namespace engine
