#pragma once

#include <memory>
#include <string>
#include <vector>

#include "engine/Camera.h"
#include "engine/Clock.h"
#include "engine/Input.h"
#include "engine/TimeControl.h"
#include "engine/Window.h"
#include "render/RenderSettings.h"
#include "render/Renderer.h"
#include "sim/Diagnostics.h"
#include "sim/GravitySystem.h"
#include "sim/SceneLibrary.h"

namespace ui {
class DebugUI;
}

namespace engine {

// Registers every configs/*.json as a scene, replacing the compiled-in preset
// of the same key. Returns how many were loaded.
int loadSceneConfigs(const std::string& directory);
// Writes every registered scene to `directory` as JSON. Used by
// --export-configs to regenerate configs/ after changing a preset in code.
int exportSceneConfigs(const std::string& directory);

// Command line surface. The headless screenshot options exist so that every
// rendering milestone can be verified from a build script rather than by
// eyeballing a live window.
struct AppOptions {
    int width = 1600;
    int height = 900;
    bool hidden = false;               // create the window but never show it
    std::string screenshotPath;        // if set: render, capture, exit
    int screenshotFrame = 30;          // which frame to capture on
    double warmupSimSeconds = 0.0;     // advance the simulation before capturing
    std::string scene = "solar-system";
    bool listScenes = false;
    bool vsync = true;
    bool noUi = false;                 // suppress the ImGui panels
    double cameraDistance = 0.0;       // overrides the scene's default
    double cameraPitch = 1e9;          // sentinel: use the scene's default
    double cameraYaw = 1e9;
    std::string focus;                 // body to frame at startup
    std::string exportConfigs;         // write every scene as JSON and exit
    // Body presets to insert after the warm-up, through exactly the same
    // spawn path the UI uses. Exists so that runtime insertion can be
    // verified from a script rather than merely asserted.
    std::vector<std::string> spawnPresets;
    double spawnDistance = 0.0;        // world units ahead of camera; 0 = scene default
    bool spawnAtRest = false;          // skip the automatic circular-orbit velocity
    double settleSimSeconds = 0.0;     // simulated seconds to advance AFTER spawning
    bool noConfigs = false;            // ignore configs/, use the built-ins
    bool noPost = false;               // draw straight to the window, no HDR/bloom
    bool noStars = false;              // hide the decorative background starfield
    bool selfTest = false;             // exercise every UI-reachable path and exit
    int benchmarkBodies = 0;           // >0: run the solver benchmark and exit
    // Deterministic frame sequence for the demo video. Simulated time is
    // advanced by a fixed amount per frame rather than by the wall clock, so
    // the same command always produces the same footage.
    std::string sequenceDir;
    int sequenceFrames = 0;
    double sequenceStepSeconds = 0.0;  // simulated seconds per output frame
    double sequenceOrbitDegrees = 0.0; // camera yaw swept over the whole clip

    static AppOptions parse(int argc, char** argv);
    static void printUsage();
};

// Runtime state the UI edits and the app acts on. Kept separate from
// RenderSettings so that "what is drawn" and "what the app is doing" do not get
// tangled together.
struct AppState {
    sim::BodyId selected = sim::kInvalidBodyId;
    sim::BodyId followed = sim::kInvalidBodyId;
    // Track the system's centre of mass instead of a single body. Following a
    // body is useless in a chaotic scene: the three-body preset ejects a star,
    // and the camera then leaves the rest of the system behind entirely.
    bool followBarycentre = false;
    std::string currentSceneKey;
    std::string statusMessage;
    double statusMessageAge = 0.0;

    // Interactive spawning (phase 19/20).
    sim::CelestialBody spawnTemplate;
    double spawnDistance = 20.0;   // world units ahead of the camera
    bool spawnOrbitAuto = true;    // give the new body a circular orbit velocity
};

class Application {
public:
    explicit Application(const AppOptions& options);
    ~Application();

    int run();

    // Accessors used by the UI layer.
    sim::GravitySystem& system() { return system_; }
    render::RenderSettings& renderSettings() { return renderSettings_; }
    TimeControl& timeControl() { return time_; }
    Camera& camera() { return camera_; }
    AppState& state() { return state_; }
    const sim::SystemDiagnostics& diagnostics() const { return diagnostics_; }
    const sim::EnergyTracker& energyTracker() const { return energyTracker_; }
    const Clock& clock() const { return clock_; }
    render::Renderer& renderer() { return renderer_; }

    void loadScene(const std::string& key);
    void resetScene();
    // Removes a body and repairs everything that referenced it: the selection,
    // the camera's focus target and the trail reference frame.
    bool deleteBody(sim::BodyId id);
    // "Reset to defaults" for each group of settings. The scene's own view and
    // physics hints are the defaults, so these re-apply the loaded preset
    // WITHOUT restarting the simulation.
    void resetRenderDefaults();
    void resetSimulationDefaults();
    void resetCameraDefaults();
    void resetAllDefaults();
    void setStatus(const std::string& message);
    sim::BodyId spawnFromCamera();
    sim::BodyId spawnBody(const sim::CelestialBody& body);
    void focusOn(sim::BodyId id);

private:
    int runSequence();
    void processInput();
    void advanceSimulation(double realDelta);
    void render();
    void updateFollowCamera();
    void applySceneView(const sim::Scene& scene);
    void resolveTrailReference(const sim::Scene& scene);
    // Drops selection/focus/trail references that no longer name a live body.
    void repairDanglingReferences();

    AppOptions options_;
    std::unique_ptr<Window> window_;
    Input input_;
    Clock clock_;
    TimeControl time_;
    Camera camera_;

    sim::GravitySystem system_;
    sim::SystemDiagnostics diagnostics_;
    sim::EnergyTracker energyTracker_;

    render::Renderer renderer_;
    render::RenderSettings renderSettings_;
    std::unique_ptr<ui::DebugUI> ui_;

    AppState state_;
    bool renderReady_ = false;
    int mergeCount_ = 0;

public:
    int mergeCount() const { return mergeCount_; }
};

}  // namespace engine
