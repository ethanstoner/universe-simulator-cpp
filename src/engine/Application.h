#pragma once

#include <memory>
#include <string>

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

    static AppOptions parse(int argc, char** argv);
    static void printUsage();
};

// Runtime state the UI edits and the app acts on. Kept separate from
// RenderSettings so that "what is drawn" and "what the app is doing" do not get
// tangled together.
struct AppState {
    sim::BodyId selected = sim::kInvalidBodyId;
    sim::BodyId followed = sim::kInvalidBodyId;
    std::string currentSceneKey;
    bool showHelp = false;
    bool wantsReset = false;
    std::string statusMessage;
    double statusMessageAge = 0.0;

    // Interactive spawning (phase 19/20).
    sim::CelestialBody spawnTemplate;
    double spawnDistance = 20.0;   // world units ahead of the camera
    bool spawnOrbitAuto = true;    // give the new body a circular orbit velocity
    int spawnPresetIndex = 2;
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
    void setStatus(const std::string& message);
    sim::BodyId spawnFromCamera();
    sim::BodyId spawnBody(const sim::CelestialBody& body);
    void focusOn(sim::BodyId id);

private:
    void processInput();
    void advanceSimulation(double realDelta);
    void render();
    void updateFollowCamera();
    void applySceneView(const sim::Scene& scene);

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
};

}  // namespace engine
