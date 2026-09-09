#include "ui/DebugUI.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "engine/Application.h"
#include "sim/Constants.h"
#include "sim/Diagnostics.h"
#include "sim/OrbitMath.h"
#include "sim/SceneLibrary.h"
#include "sim/Units.h"

namespace ui {
namespace {

void labelledValue(const char* label, const std::string& value) {
    ImGui::TextUnformatted(label);
    ImGui::SameLine(168.0f);
    ImGui::TextUnformatted(value.c_str());
}

void labelledValue(const char* label, const char* format, double value) {
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), format, value);
    labelledValue(label, std::string(buffer));
}

std::string vectorText(const sim::Vec3& v, const char* unit) {
    char buffer[160];
    std::snprintf(buffer, sizeof(buffer), "%.6g, %.6g, %.6g %s", v.x, v.y, v.z, unit);
    return buffer;
}

// Panels are laid out against the live viewport size rather than hard-coded
// pixel positions, so they stay on screen at 1280x720 as well as 2560x1440.
enum class PanelSlot { LeftTop, LeftMiddle, LeftBottom, RightTop, RightBottom,
                       Floating, FloatingLower };

void placePanel(PanelSlot slot, float preferredHeight) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float width = viewport->WorkSize.x;
    const float height = viewport->WorkSize.y;
    const float menuBar = 30.0f;
    const float margin = 10.0f;
    const float panelWidth = std::min(376.0f, width * 0.30f);
    const float usable = height - menuBar - margin * 2.0f;

    float x = margin;
    float y = menuBar + margin;
    float h = preferredHeight;

    switch (slot) {
        case PanelSlot::LeftTop:
            h = std::min(preferredHeight, usable * 0.34f);
            break;
        case PanelSlot::LeftMiddle:
            h = std::min(preferredHeight, usable * 0.38f);
            y += usable * 0.34f + margin;
            break;
        case PanelSlot::LeftBottom:
            h = std::min(preferredHeight, usable * 0.26f);
            y += usable * 0.72f + margin * 2.0f;
            break;
        case PanelSlot::RightTop:
            x = width - panelWidth - margin;
            h = std::min(preferredHeight, usable * 0.55f);
            break;
        case PanelSlot::RightBottom:
            x = width - panelWidth - margin;
            h = std::min(preferredHeight, usable * 0.43f);
            y += usable * 0.55f + margin;
            break;
        case PanelSlot::Floating:
            x = margin + panelWidth + margin;
            h = std::min(preferredHeight, usable * 0.60f);
            break;
        case PanelSlot::FloatingLower:
            x = margin + panelWidth + margin;
            h = std::min(preferredHeight, usable * 0.36f);
            y += usable * 0.62f;
            break;
    }

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + x, viewport->WorkPos.y + y),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, h), ImGuiCond_FirstUseEver);
}

// ImGui places a widget's label to its RIGHT and defaults the widget to the
// full content width, so labels ran off the edge of the panel. Reserving a
// fixed label column fixes every widget in the panel at once.
void beginPanelBody() { ImGui::PushItemWidth(-150.0f); }
void endPanelBody() { ImGui::PopItemWidth(); }

// Time scale presets, with labels written out rather than printf'd: "%.0g"
// renders 1e4 as "1e+04x", which is both ugly and wider than the button.
struct TimeScalePreset {
    const char* label;
    double value;
};
constexpr TimeScalePreset kTimeScales[] = {
    {"0.1x", 0.1},   {"1x", 1.0},     {"10x", 10.0},   {"100x", 100.0},
    {"1k", 1.0e3},   {"10k", 1.0e4},  {"100k", 1.0e5}, {"1M", 1.0e6},
    {"10M", 1.0e7},  {"100M", 1.0e8},
};
constexpr int kTimeScaleCount = static_cast<int>(sizeof(kTimeScales) / sizeof(kTimeScales[0]));

}  // namespace

void DebugUI::initialize(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // do not litter the working directory
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.06f, 0.09f, 0.92f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.16f, 0.26f, 1.00f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.16f, 0.22f, 0.34f, 0.80f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.16f, 0.22f, 0.34f, 0.90f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.13f, 0.20f, 0.90f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    initialized_ = true;
}

void DebugUI::shutdown() {
    if (!initialized_) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    initialized_ = false;
}

bool DebugUI::wantsKeyboard() const {
    return initialized_ && ImGui::GetIO().WantCaptureKeyboard;
}
bool DebugUI::wantsMouse() const {
    return initialized_ && ImGui::GetIO().WantCaptureMouse;
}

void DebugUI::beginFrame() {
    if (!initialized_) return;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void DebugUI::endFrame() {
    if (!initialized_) return;
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void DebugUI::build(engine::Application& app) {
    if (!initialized_) return;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Panels")) {
            ImGui::MenuItem("Simulation", nullptr, &showSimulation_);
            ImGui::MenuItem("System", nullptr, &showSystem_);
            ImGui::MenuItem("Rendering", nullptr, &showRendering_);
            ImGui::MenuItem("Bodies", nullptr, &showBodies_);
            ImGui::MenuItem("Spawn", nullptr, &showSpawn_);
            ImGui::MenuItem("Scenes", nullptr, &showScenes_);
            ImGui::MenuItem("Help", nullptr, &showHelp_);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Reset")) {
            if (ImGui::MenuItem("Restart scene", "R")) app.resetScene();
            ImGui::Separator();
            if (ImGui::MenuItem("Rendering settings")) app.resetRenderDefaults();
            if (ImGui::MenuItem("Simulation settings")) app.resetSimulationDefaults();
            if (ImGui::MenuItem("Camera")) app.resetCameraDefaults();
            ImGui::Separator();
            if (ImGui::MenuItem("Everything")) {
                app.resetAllDefaults();
                app.resetScene();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Resets all settings AND restarts the scene.");
            }
            if (ImGui::MenuItem("Show all panels")) {
                showSimulation_ = showSystem_ = showRendering_ = true;
                showBodies_ = showScenes_ = true;
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        ImGui::Text("%.0f FPS", app.clock().fps());
        ImGui::Separator();
        ImGui::Text("t = %s", sim::formatDuration(
                                  app.system().elapsedSimulatedSeconds()).c_str());
        ImGui::Separator();
        ImGui::Text("%zu bodies", app.system().size());
        if (!app.state().statusMessage.empty() && app.state().statusMessageAge < 4.0) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.0f), "%s",
                               app.state().statusMessage.c_str());
        }
        ImGui::EndMainMenuBar();
    }

    if (app.renderSettings().showLabels) bodyLabels(app);
    if (showScenes_) scenePanel(app);
    if (showSimulation_) simulationPanel(app);
    if (showSystem_) systemPanel(app);
    if (showRendering_) renderingPanel(app);
    if (showBodies_) bodiesPanel(app);
    if (showSpawn_) spawnPanel(app);
    if (showHelp_) helpOverlay(app);
}

void DebugUI::scenePanel(engine::Application& app) {
    placePanel(PanelSlot::LeftTop, 300.0f);
    if (!ImGui::Begin("Scenes", &showScenes_)) {
        ImGui::End();
        return;
    }

    const std::vector<sim::Scene>& scenes = sim::builtinScenes();
    for (const sim::Scene& scene : scenes) {
        const bool current = scene.key == app.state().currentSceneKey;
        if (current) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.42f, 0.30f, 1.0f));
        if (ImGui::Button(scene.title.c_str(), ImVec2(-1.0f, 0.0f))) {
            app.loadScene(scene.key);
        }
        if (current) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", scene.description.c_str());
        }
    }

    ImGui::Separator();
    if (const sim::Scene* scene = sim::findScene(app.state().currentSceneKey)) {
        ImGui::TextWrapped("%s", scene->description.c_str());
    }
    ImGui::End();
}

void DebugUI::simulationPanel(engine::Application& app) {
    engine::TimeControl& time = app.timeControl();

    placePanel(PanelSlot::LeftMiddle, 330.0f);
    if (!ImGui::Begin("Simulation", &showSimulation_)) {
        ImGui::End();
        return;
    }

    beginPanelBody();

    if (ImGui::Button(time.paused ? "Resume" : "Pause", ImVec2(90, 0))) {
        time.paused = !time.paused;
    }
    ImGui::SameLine();
    if (ImGui::Button("Step", ImVec2(70, 0))) {
        time.paused = true;
        time.singleStepRequested = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset", ImVec2(70, 0))) app.resetScene();

    ImGui::Separator();

    ImGui::TextUnformatted("Time scale (simulated seconds per real second)");
    float exponent = static_cast<float>(std::log10(std::max(time.timeScale, 1e-6)));
    if (ImGui::SliderFloat("##timescale", &exponent, -2.0f, 9.0f, "1e%.2f")) {
        time.timeScale = std::pow(10.0, static_cast<double>(exponent));
    }
    ImGui::Text("= %s of simulated time per second",
                sim::formatDuration(time.timeScale).c_str());

    for (int i = 0; i < kTimeScaleCount; ++i) {
        if (i % 5 != 0) ImGui::SameLine();
        if (ImGui::SmallButton(kTimeScales[i].label)) time.timeScale = kTimeScales[i].value;
    }

    ImGui::Separator();

    float fixedDt = static_cast<float>(time.fixedDt);
    if (ImGui::InputFloat("Fixed step (s)", &fixedDt, 0.0f, 0.0f, "%.4f")) {
        if (fixedDt > 0.0f) time.fixedDt = fixedDt;
    }
    ImGui::SetItemTooltip(
        "Simulated seconds advanced per physics step. Time acceleration adds "
        "steps rather than enlarging this, so raising the time scale does not "
        "degrade orbital accuracy.");

    ImGui::SliderInt("Max steps/frame", &time.maxStepsPerFrame, 1, 20000);
    labelledValue("Steps last frame", "%.0f", static_cast<double>(time.stepsLastFrame));
    labelledValue("Physics steps/sec", "%.0f", time.stepsPerSecond);
    if (time.budgetExceeded) {
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
                           "Step budget hit: running slower than the requested "
                           "time scale.");
    }

    ImGui::Separator();

    sim::SimulationSettings& settings = app.system().settings();
    int integrator = static_cast<int>(settings.integrator);
    const char* integratorNames[] = {"Explicit Euler", "Symplectic Euler",
                                     "Velocity Verlet", "Runge-Kutta 4"};
    if (ImGui::Combo("Integrator", &integrator, integratorNames, 4)) {
        settings.integrator = static_cast<sim::IntegratorType>(integrator);
        app.system().invalidate();
    }
    if (!sim::isSymplectic(settings.integrator)) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f),
                           "Not symplectic: energy error accumulates over long runs.");
    }

    int stabilization = static_cast<int>(settings.stabilization);
    const char* stabilizationNames[] = {"None (raw 1/r^2)", "Plummer softening",
                                        "Minimum distance"};
    if (ImGui::Combo("Stabilisation", &stabilization, stabilizationNames, 3)) {
        settings.stabilization = static_cast<sim::Stabilization>(stabilization);
    }
    if (settings.stabilization == sim::Stabilization::Softening) {
        float softening = static_cast<float>(settings.softeningLength / 1000.0);
        if (ImGui::InputFloat("Softening (km)", &softening, 0.0f, 0.0f, "%.1f")) {
            settings.softeningLength = std::max(0.0, static_cast<double>(softening) * 1000.0);
        }
        ImGui::SetItemTooltip(
            "a = G m r / (r^2 + eps^2)^(3/2). Bounds the force at short range so "
            "a near-miss cannot produce an infinite acceleration.");
    } else if (settings.stabilization == sim::Stabilization::MinDistance) {
        float minimum = static_cast<float>(settings.minimumDistance / 1000.0);
        if (ImGui::InputFloat("Minimum distance (km)", &minimum, 0.0f, 0.0f, "%.1f")) {
            settings.minimumDistance = std::max(0.0, static_cast<double>(minimum) * 1000.0);
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
                           "Unstabilised: a direct hit produces infinities.");
    }

    int collision = static_cast<int>(settings.collisionMode);
    const char* collisionNames[] = {"Ignore", "Elastic", "Merge"};
    if (ImGui::Combo("Collisions", &collision, collisionNames, 3)) {
        settings.collisionMode = static_cast<sim::CollisionMode>(collision);
    }
    if (settings.collisionMode == sim::CollisionMode::Elastic) {
        float restitution = static_cast<float>(settings.collisionRestitution);
        if (ImGui::SliderFloat("Restitution", &restitution, 0.0f, 1.0f)) {
            settings.collisionRestitution = restitution;
        }
    }
    if (settings.collisionMode != sim::CollisionMode::Ignore) {
        float radiusScale = static_cast<float>(settings.collisionRadiusScale);
        if (ImGui::SliderFloat("Contact radius x", &radiusScale, 1.0f, 500.0f, "%.0f",
                               ImGuiSliderFlags_Logarithmic)) {
            settings.collisionRadiusScale = radiusScale;
        }
        ImGui::SetItemTooltip(
            "Real planetary radii are minute next to orbital separations, so "
            "contact almost never happens at 1x. Raise this to make collisions "
            "reachable in a demonstration.");
    }

    int solver = static_cast<int>(settings.solver);
    const char* solverNames[] = {"Direct O(N^2)", "Barnes-Hut O(N log N)"};
    if (ImGui::Combo("Solver", &solver, solverNames, 2)) {
        settings.solver = static_cast<sim::GravitySolver>(solver);
        app.system().invalidate();
    }
    ImGui::SetItemTooltip(
        "Direct summation visits every pair once and applies equal-and-opposite "
        "accelerations, so momentum is conserved to rounding. Barnes-Hut groups "
        "distant bodies into their centre of mass, which is much faster above a "
        "few thousand bodies but breaks that exact pairing.");

    if (settings.solver == sim::GravitySolver::BarnesHut) {
        float theta = static_cast<float>(settings.barnesHutTheta);
        if (ImGui::SliderFloat("Opening angle", &theta, 0.0f, 1.5f, "%.2f")) {
            settings.barnesHutTheta = theta;
        }
        ImGui::SetItemTooltip(
            "A cell of width s at distance d is treated as one mass when "
            "s/d < theta. 0 is exact and as slow as direct summation, 0.5 is "
            "about 1% force error, larger is faster and progressively wronger.");
        ImGui::TextColored(ImVec4(0.75f, 0.8f, 0.95f, 1.0f),
                           "Approximate: momentum is conserved only to the\n"
                           "opening-angle error. Use Direct for long runs.");
    }

    ImGui::SeparatorText("Relativity");
    if (ImGui::Checkbox("1PN correction", &settings.relativisticCorrection)) {
        app.system().invalidate();
    }
    ImGui::SetItemTooltip(
        "Adds the leading post-Newtonian term. At strength 1 it advances "
        "Mercury's perihelion by 43 arcseconds per century, which the test "
        "suite measures as 42.77. Off by default: the simulator is Newtonian "
        "unless you ask for this.");
    if (settings.relativisticCorrection) {
        float strength = static_cast<float>(settings.relativisticStrength);
        if (ImGui::SliderFloat("Strength", &strength, 1.0f, 1.0e6f, "%.0f",
                               ImGuiSliderFlags_Logarithmic)) {
            settings.relativisticStrength = strength;
        }
        ImGui::SetItemTooltip(
            "1 is physical. Anything above it exaggerates the effect so the "
            "precession is visible in seconds rather than centuries, which is a "
            "demonstration setting and not physics.");
        if (settings.relativisticStrength > 1.5) {
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.4f, 1.0f),
                               "Exaggerated %.0fx: for demonstration, not physical.",
                               settings.relativisticStrength);
        }
        ImGui::TextColored(ImVec4(0.75f, 0.8f, 0.95f, 1.0f),
                           "Two-body Schwarzschild term applied pairwise,\n"
                           "not the full N-body EIH Lagrangian.");
    }

    ImGui::Checkbox("Pairwise gravity", &settings.pairwiseGravityEnabled);

    ImGui::Separator();
    if (ImGui::Button("Reset simulation settings to defaults", ImVec2(-1.0f, 0.0f))) {
        app.resetSimulationDefaults();
    }
    ImGui::SetItemTooltip(
        "Restores this scene's integrator, stabilisation, collision mode, step "
        "size and time scale. Body positions and velocities are left alone, so "
        "the run continues -- use Reset above to restart the scene itself.");

    endPanelBody();
    ImGui::End();
}

void DebugUI::systemPanel(engine::Application& app) {
    const sim::SystemDiagnostics& d = app.diagnostics();
    const sim::EnergyTracker& tracker = app.energyTracker();

    placePanel(PanelSlot::LeftBottom, 250.0f);
    if (!ImGui::Begin("System diagnostics", &showSystem_)) {
        ImGui::End();
        return;
    }

    labelledValue("Bodies", "%.0f", static_cast<double>(app.system().size()));
    labelledValue("Total mass", sim::formatMass(d.totalMass));
    labelledValue("Simulated time",
                  sim::formatDuration(app.system().elapsedSimulatedSeconds()));
    labelledValue("Physics steps", "%.0f",
                  static_cast<double>(app.system().stepCount()));
    if (app.mergeCount() > 0) {
        // Surfaced rather than silent: otherwise bodies just vanish from the
        // list and it looks like a bug.
        labelledValue("Bodies merged", "%.0f", static_cast<double>(app.mergeCount()));
    }

    ImGui::Separator();
    labelledValue("Kinetic energy", sim::formatEnergy(d.kineticEnergy));
    labelledValue("Potential energy", sim::formatEnergy(d.potentialEnergy));
    labelledValue("Total energy", sim::formatEnergy(d.totalEnergy));

    const double drift = tracker.relativeDrift();
    const ImVec4 driftColor = drift > 1e-3 ? ImVec4(1.0f, 0.45f, 0.35f, 1.0f)
                                           : ImVec4(0.55f, 0.95f, 0.6f, 1.0f);
    ImGui::TextUnformatted("Energy drift");
    ImGui::SameLine(168.0f);
    ImGui::TextColored(driftColor, "%.3e", drift);
    ImGui::SetItemTooltip(
        "|E - E0| / |E0| against the energy when the scene was loaded. A "
        "symplectic integrator keeps this bounded and oscillating; a steadily "
        "growing value means the orbits are decaying numerically.");
    labelledValue("Peak drift", "%.3e", tracker.peakRelativeDrift());

    ImGui::Separator();
    labelledValue("Momentum", vectorText(d.linearMomentum, "kg m/s"));
    labelledValue("Angular momentum", vectorText(d.angularMomentum, "kg m^2/s"));
    labelledValue("Barycentre", vectorText(d.centreOfMass, "m"));

    ImGui::End();
}

void DebugUI::renderingPanel(engine::Application& app) {
    render::RenderSettings& settings = app.renderSettings();

    placePanel(PanelSlot::RightTop, 470.0f);
    if (!ImGui::Begin("Rendering", &showRendering_)) {
        ImGui::End();
        return;
    }

    beginPanelBody();

    ImGui::Checkbox("Bodies", &settings.showBodies);
    ImGui::SameLine();
    ImGui::Checkbox("Trails", &settings.showTrails);
    ImGui::SameLine();
    ImGui::Checkbox("Grid", &settings.showGrid);
    ImGui::Checkbox("Labels", &settings.showLabels);
    ImGui::SameLine();
    ImGui::Checkbox("Wireframe", &settings.wireframe);

    ImGui::Checkbox("Predicted path", &settings.showTrajectory);
    ImGui::SetItemTooltip(
        "Forecasts the selected body by integrating a copy of the whole system "
        "with the same integrator, so it includes perturbation from every other "
        "body. It is a prediction under current conditions: spawning a body or "
        "editing an orbit changes it.");
    if (settings.showTrajectory) {
        float years = static_cast<float>(settings.trajectoryHorizonYears);
        if (ImGui::SliderFloat("Forecast (years)", &years, 0.01f, 50.0f, "%.2f",
                               ImGuiSliderFlags_Logarithmic)) {
            settings.trajectoryHorizonYears = years;
        }
        ImGui::SliderInt("Forecast samples", &settings.trajectorySamples, 50, 4000);
    }
    ImGui::Checkbox("Velocity vectors", &settings.showVelocityVectors);
    ImGui::Checkbox("Acceleration vectors", &settings.showAccelerationVectors);
    ImGui::Checkbox("Schwarzschild radius", &settings.showSchwarzschildRadius);
    ImGui::SetItemTooltip(
        "Drawn at true scale with no exaggeration, so for ordinary bodies it is "
        "far too small to see -- the Sun's is 2.95 km against a 696 000 km "
        "radius. It is a reference figure, not an event horizon: this simulator "
        "is Newtonian.");

    ImGui::SeparatorText("Scale");
    ImGui::Checkbox("True scale", &settings.trueScale);
    ImGui::SetItemTooltip(
        "Draw bodies at their real relative size. Worth looking at once: at 1 AU "
        "= 10 world units the Earth is 4e-4 units across and simply disappears. "
        "That is why the default exaggerates.");

    if (!settings.trueScale) {
        float gain = static_cast<float>(settings.bodyVisualGain);
        if (ImGui::SliderFloat("Body size gain", &gain, 0.01f, 200.0f, "%.3f",
                               ImGuiSliderFlags_Logarithmic)) {
            settings.bodyVisualGain = gain;
        }
        float exponent = static_cast<float>(settings.bodyVisualExponent);
        if (ImGui::SliderFloat("Size exponent", &exponent, 0.05f, 1.0f, "%.3f")) {
            settings.bodyVisualExponent = exponent;
        }
        ImGui::SetItemTooltip(
            "drawn = gain * (trueRadius / metresPerUnit) ^ exponent.\n"
            "At exponent 1 this is a plain multiplier, which cannot work here: "
            "the Sun is 109 Earth radii, so any factor that makes the Earth "
            "visible makes the Sun wider than the Earth's orbit. Below 1 the "
            "ratio is compressed while the ordering is kept.\n"
            "Display only -- gravity never reads the drawn radius.");
    }

    float minRadius = static_cast<float>(settings.minVisualRadius);
    if (ImGui::SliderFloat("Min drawn radius", &minRadius, 0.0f, 1.0f, "%.3f")) {
        settings.minVisualRadius = minRadius;
    }
    labelledValue("Metres per unit", "%.6g", settings.metresPerUnit);
    labelledValue("1 world unit", sim::formatDistance(settings.metresPerUnit));

    ImGui::SeparatorText("Trails");
    ImGui::SliderFloat("Trail opacity", &settings.trailOpacity, 0.0f, 1.0f);
    ImGui::SliderInt("Trail samples drawn", &settings.trailMaxSamples, 2, 4000);
    int historyLength = static_cast<int>(app.system().settings().trailLength);
    if (ImGui::SliderInt("Trail history", &historyLength, 0, 6000)) {
        app.system().settings().trailLength = static_cast<std::size_t>(historyLength);
    }
    if (ImGui::Button("Clear trails")) app.system().clearTrails();

    ImGui::SeparatorText("Spacetime grid (visualisation)");
    ImGui::TextWrapped(
        "A rubber-sheet analogy, not general relativity. The sheet is displaced "
        "by the softened Newtonian potential of each body. Nothing in the "
        "simulation reads it.");
    ImGui::SliderFloat("Grid strength", &settings.gridStrength, 0.0f, 4.0f);
    ImGui::SliderFloat("Grid opacity", &settings.gridOpacity, 0.0f, 1.0f);
    ImGui::SliderInt("Grid resolution", &settings.gridResolution, 8, 400);
    float extent = static_cast<float>(settings.gridExtent);
    if (ImGui::SliderFloat("Grid extent", &extent, 2.0f, 500.0f, "%.1f",
                           ImGuiSliderFlags_Logarithmic)) {
        settings.gridExtent = extent;
    }
    float maxDepth = static_cast<float>(settings.gridMaxDepth);
    if (ImGui::SliderFloat("Max well depth", &maxDepth, 0.2f, 80.0f, "%.2f",
                           ImGuiSliderFlags_Logarithmic)) {
        settings.gridMaxDepth = maxDepth;
    }
    ImGui::SliderFloat("Well softening", &settings.gridWellSoftening, 0.001f, 0.25f,
                       "%.4f", ImGuiSliderFlags_Logarithmic);
    ImGui::Checkbox("Grid follows camera", &settings.gridFollowsCamera);
    ImGui::SameLine();
    ImGui::Checkbox("Shaded sheet", &settings.gridShaded);

    ImGui::SeparatorText("Look");
    ImGui::Checkbox("Starfield", &settings.showStarfield);
    ImGui::SetItemTooltip(
        "Decorative background only. These are not simulated bodies and not a "
        "star catalogue; they are generated from a fixed seed so the sky is the "
        "same on every run.");
    if (settings.showStarfield) {
        ImGui::SliderFloat("Star brightness", &settings.starfieldBrightness, 0.0f, 2.5f);
        ImGui::SliderFloat("Star size", &settings.starfieldSize, 0.5f, 5.0f);
    }

    ImGui::Checkbox("HDR post-processing", &settings.postProcessEnabled);
    ImGui::SetItemTooltip(
        "Renders the scene into a floating-point buffer so a star's core can be "
        "brighter than white. Turning this off draws straight to the window and "
        "loses bloom and tone mapping.");
    if (settings.postProcessEnabled) {
        ImGui::Checkbox("Bloom", &settings.bloomEnabled);
        if (settings.bloomEnabled) {
            ImGui::SliderFloat("Bloom intensity", &settings.bloomIntensity, 0.0f, 2.0f);
            ImGui::SliderFloat("Bloom threshold", &settings.bloomThreshold, 0.1f, 4.0f);
            ImGui::SliderFloat("Bloom knee", &settings.bloomSoftKnee, 0.0f, 1.5f);
            ImGui::SliderInt("Bloom blur passes", &settings.bloomIterations, 1, 12);
        }
        ImGui::SliderFloat("Exposure", &settings.exposure, 0.1f, 4.0f);
        ImGui::Checkbox("ACES tone map", &settings.tonemap);
        ImGui::SliderFloat("Vignette", &settings.vignette, 0.0f, 1.0f);
    }

    ImGui::SeparatorText("Quality");
    ImGui::SliderInt("Sphere latitude", &settings.sphereLatitudeSegments, 3, 96);
    ImGui::SliderInt("Sphere longitude", &settings.sphereLongitudeSegments, 3, 192);
    ImGui::SliderFloat("Ambient", &settings.ambient, 0.0f, 0.5f);
    ImGui::SliderFloat("Star brightness", &settings.starBrightness, 0.1f, 3.0f);

    ImGui::Separator();
    if (ImGui::Button("Reset rendering to defaults", ImVec2(-1.0f, 0.0f))) {
        app.resetRenderDefaults();
    }
    ImGui::SetItemTooltip(
        "Restores every display setting on this panel, then re-applies the "
        "current scene's scale and framing hints.");

    ImGui::SeparatorText("Camera");
    engine::Camera& camera = app.camera();
    int mode = camera.mode() == engine::CameraMode::Orbit ? 0 : 1;
    const char* modes[] = {"Orbit (follow body)", "Free flight"};
    if (ImGui::Combo("Mode", &mode, modes, 2)) {
        camera.setMode(mode == 0 ? engine::CameraMode::Orbit : engine::CameraMode::Fly);
    }
    ImGui::SliderFloat("Field of view", &camera.fovDegrees, 20.0f, 110.0f, "%.0f deg");
    ImGui::SliderFloat("Fly speed", &camera.moveSpeed, 0.01f, 5000.0f, "%.2f",
                       ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Look sensitivity", &camera.mouseSensitivity, 0.01f, 0.5f);
    if (camera.mode() == engine::CameraMode::Orbit) {
        float distance = static_cast<float>(camera.orbitDistance());
        if (ImGui::SliderFloat("Orbit distance", &distance, 0.02f, 2000.0f, "%.3f",
                               ImGuiSliderFlags_Logarithmic)) {
            camera.setOrbitDistance(distance);
        }
    }
    if (camera.mode() == engine::CameraMode::Orbit) {
        if (ImGui::Checkbox("Follow barycentre", &app.state().followBarycentre)) {
            if (!app.state().followBarycentre && !app.system().bodies().empty()) {
                app.state().followed = app.state().selected;
            }
        }
        ImGui::SetItemTooltip(
            "Track the system's centre of mass instead of one body. Following a "
            "single body is useless in a chaotic scene: three-body ejects a star "
            "and the camera leaves everything else behind.");
    }
    if (ImGui::Button("Reset camera", ImVec2(-1.0f, 0.0f))) app.resetCameraDefaults();

    endPanelBody();
    ImGui::End();
}

void DebugUI::bodiesPanel(engine::Application& app) {
    placePanel(PanelSlot::RightBottom, 420.0f);
    if (!ImGui::Begin("Bodies", &showBodies_)) {
        ImGui::End();
        return;
    }

    beginPanelBody();

    if (ImGui::BeginChild("body-list", ImVec2(0, 150), ImGuiChildFlags_Border)) {
        for (const sim::CelestialBody& body : app.system().bodies()) {
            ImGui::PushID(static_cast<int>(body.id));
            const bool selected = body.id == app.state().selected;
            ImGui::ColorButton("##color",
                               ImVec4(body.color.r, body.color.g, body.color.b, 1.0f),
                               ImGuiColorEditFlags_NoTooltip, ImVec2(14, 14));
            ImGui::SameLine();
            if (ImGui::Selectable(body.name.c_str(), selected)) {
                app.state().selected = body.id;
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                app.focusOn(body.id);
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    if (ImGui::Button("Focus (F)")) app.focusOn(app.state().selected);
    ImGui::SameLine();
    if (ImGui::Button("Delete")) app.deleteBody(app.state().selected);
    ImGui::SameLine();
    if (ImGui::Button("Duplicate")) {
        if (const sim::CelestialBody* body = app.system().find(app.state().selected)) {
            // Everything needed is copied out FIRST. spawnBody() push_backs into
            // the body vector, which can reallocate and leave `body` dangling --
            // reading body->name after the call was a use-after-free.
            sim::CelestialBody copy = *body;
            const std::string originalName = body->name;
            copy.name = originalName + " copy";
            // Offset so the duplicate is not created exactly on top of the
            // original, which would be an immediate collision.
            copy.position += sim::Vec3(copy.radius * 4.0 + 1.0e7, 0.0, 0.0);
            copy.trail.clear();
            app.spawnBody(copy);
            app.setStatus("Duplicated " + originalName);
        }
    }

    ImGui::Separator();
    inspectorPanel(app);
    endPanelBody();
    ImGui::End();
}

void DebugUI::inspectorPanel(engine::Application& app) {
    sim::CelestialBody* body = app.system().find(app.state().selected);
    if (!body) {
        ImGui::TextDisabled("No body selected. Click one in the viewport, or Tab.");
        return;
    }

    ImGui::SeparatorText(body->name.c_str());

    char nameBuffer[128];
    std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", body->name.c_str());
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        body->name = nameBuffer;
    }

    ImGui::ColorEdit3("Colour", &body->color.x, ImGuiColorEditFlags_NoInputs);
    ImGui::SameLine();
    ImGui::Checkbox("Star", &body->emissive);
    ImGui::Checkbox("Trail", &body->showTrail);
    ImGui::SameLine();
    if (ImGui::Checkbox("Fixed", &body->fixed)) app.system().invalidate();
    ImGui::SetItemTooltip("A fixed body still attracts others but never moves.");

    double massEarths = body->mass / sim::constants::kEarthMass;
    if (ImGui::InputDouble("Mass (Earths)", &massEarths, 0.0, 0.0, "%.6g")) {
        body->mass = std::max(0.0, massEarths) * sim::constants::kEarthMass;
        app.system().invalidate();
    }
    labelledValue("Mass", sim::formatMass(body->mass));

    double radiusKm = body->radius / 1000.0;
    if (ImGui::InputDouble("Radius (km)", &radiusKm, 0.0, 0.0, "%.6g")) {
        body->radius = std::max(0.0, radiusKm) * 1000.0;
    }
    labelledValue("Physical radius", sim::formatDistance(body->radius));
    labelledValue("Drawn radius", "%.4f units",
                  render::Renderer::visualRadius(*body, app.renderSettings()));
    labelledValue("Mean density", "%.4g kg/m^3", body->density());

    ImGui::SeparatorText("State");
    if (ImGui::InputDouble("x (m)", &body->position.x, 0.0, 0.0, "%.6g")) {
        app.system().invalidate();
    }
    if (ImGui::InputDouble("y (m)", &body->position.y, 0.0, 0.0, "%.6g")) {
        app.system().invalidate();
    }
    if (ImGui::InputDouble("z (m)", &body->position.z, 0.0, 0.0, "%.6g")) {
        app.system().invalidate();
    }
    if (ImGui::InputDouble("vx (m/s)", &body->velocity.x, 0.0, 0.0, "%.6g")) {
        app.system().invalidate();
    }
    if (ImGui::InputDouble("vy (m/s)", &body->velocity.y, 0.0, 0.0, "%.6g")) {
        app.system().invalidate();
    }
    if (ImGui::InputDouble("vz (m/s)", &body->velocity.z, 0.0, 0.0, "%.6g")) {
        app.system().invalidate();
    }

    labelledValue("Speed", sim::formatSpeed(body->speed()));
    labelledValue("Acceleration", "%.6g m/s^2", glm::length(body->acceleration));
    labelledValue("Kinetic energy", sim::formatEnergy(body->kineticEnergy()));
    labelledValue("Schwarzschild r", sim::formatDistance(
                                         sim::schwarzschildRadius(body->mass)));
    ImGui::SetItemTooltip(
        "r_s = 2GM/c^2, shown for reference. Nothing in this simulation "
        "behaves differently at that radius: the dynamics are Newtonian.");

    // Relationship to the heaviest other body, which is the useful reference in
    // practice (the Sun, or whichever star dominates).
    const sim::CelestialBody* primary = nullptr;
    for (const sim::CelestialBody& candidate : app.system().bodies()) {
        if (candidate.id == body->id) continue;
        if (!primary || candidate.mass > primary->mass) primary = &candidate;
    }
    if (primary) {
        ImGui::SeparatorText(("Relative to " + primary->name).c_str());
        const sim::Vec3 relativePosition = body->position - primary->position;
        const sim::Vec3 relativeVelocity = body->velocity - primary->velocity;
        labelledValue("Distance", sim::formatDistance(glm::length(relativePosition)));
        labelledValue("Relative speed", sim::formatSpeed(glm::length(relativeVelocity)));

        const double mu = app.system().settings().gravitationalConstant *
                          (primary->mass + body->mass);
        const sim::OrbitalElements elements =
            sim::computeOrbitalElements(relativePosition, relativeVelocity, mu);
        if (elements.valid) {
            if (elements.bound) {
                labelledValue("Semi-major axis", sim::formatDistance(elements.semiMajorAxis));
                labelledValue("Eccentricity", "%.5f", elements.eccentricity);
                labelledValue("Periapsis", sim::formatDistance(elements.periapsis));
                labelledValue("Apoapsis", sim::formatDistance(elements.apoapsis));
                labelledValue("Period", sim::formatDuration(elements.period));
                labelledValue("Inclination", "%.3f deg",
                              elements.inclination * 180.0 / 3.14159265358979);
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f),
                                   "Unbound (e = %.3f): this body is escaping.",
                                   elements.eccentricity);
            }
        }
    }
}

void DebugUI::spawnPanel(engine::Application& app) {
    engine::AppState& state = app.state();

    placePanel(PanelSlot::Floating, 420.0f);
    if (!ImGui::Begin("Spawn a body", &showSpawn_)) {
        ImGui::End();
        return;
    }

    beginPanelBody();

    ImGui::TextUnformatted("Presets");
    // Shared with the --spawn command line option, so the scripted spawns used
    // to verify runtime insertion use exactly these definitions.
    const std::vector<sim::BodyPreset>& presets = sim::bodyPresets();
    for (std::size_t i = 0; i < presets.size(); ++i) {
        if (i % 2 != 0) ImGui::SameLine();
        if (ImGui::Button(presets[i].name.c_str(), ImVec2(160, 0))) {
            state.spawnTemplate.name = presets[i].name;
            state.spawnTemplate.mass = presets[i].mass;
            state.spawnTemplate.radius = presets[i].radius;
            state.spawnTemplate.color = presets[i].color;
            state.spawnTemplate.emissive = presets[i].emissive;
        }
        if (ImGui::IsItemHovered() && !presets[i].note.empty()) {
            ImGui::SetTooltip("%s", presets[i].note.c_str());
        }
    }

    ImGui::Separator();

    char nameBuffer[128];
    std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", state.spawnTemplate.name.c_str());
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        state.spawnTemplate.name = nameBuffer;
    }

    double massSolar = state.spawnTemplate.mass / sim::constants::kSolarMass;
    if (ImGui::InputDouble("Mass (solar)", &massSolar, 0.0, 0.0, "%.6g")) {
        state.spawnTemplate.mass = std::max(0.0, massSolar) * sim::constants::kSolarMass;
    }
    double massEarth = state.spawnTemplate.mass / sim::constants::kEarthMass;
    if (ImGui::InputDouble("Mass (Earths)", &massEarth, 0.0, 0.0, "%.6g")) {
        state.spawnTemplate.mass = std::max(0.0, massEarth) * sim::constants::kEarthMass;
    }
    double radiusKm = state.spawnTemplate.radius / 1000.0;
    if (ImGui::InputDouble("Radius (km)", &radiusKm, 0.0, 0.0, "%.6g")) {
        state.spawnTemplate.radius = std::max(0.0, radiusKm) * 1000.0;
    }
    ImGui::ColorEdit3("Colour", &state.spawnTemplate.color.x, ImGuiColorEditFlags_NoInputs);
    ImGui::SameLine();
    ImGui::Checkbox("Star", &state.spawnTemplate.emissive);

    labelledValue("Density", "%.4g kg/m^3", state.spawnTemplate.density());
    labelledValue("Schwarzschild r",
                  sim::formatDistance(sim::schwarzschildRadius(state.spawnTemplate.mass)));
    if (state.spawnTemplate.radius > 0.0 &&
        sim::schwarzschildRadius(state.spawnTemplate.mass) > state.spawnTemplate.radius) {
        ImGui::TextColored(
            ImVec4(1.0f, 0.55f, 0.4f, 1.0f),
            "This body is inside its own Schwarzschild radius. In reality that "
            "means a black hole; here it is still a Newtonian point mass.");
    }

    ImGui::SeparatorText("Placement");
    ImGui::TextWrapped(
        "Spawns along the camera's forward axis. Aim the view, set the "
        "distance, and press Spawn (or N in the viewport).");
    float distance = static_cast<float>(state.spawnDistance);
    if (ImGui::SliderFloat("Distance (units)", &distance, 0.1f, 500.0f, "%.2f",
                           ImGuiSliderFlags_Logarithmic)) {
        state.spawnDistance = distance;
    }
    labelledValue("= distance", sim::formatDistance(
                                    state.spawnDistance * app.renderSettings().metresPerUnit));
    ImGui::Checkbox("Give it a circular orbit", &state.spawnOrbitAuto);
    ImGui::SetItemTooltip(
        "Sets the velocity to sqrt(GM/r) about the heaviest body, perpendicular "
        "to the radius. Without this the new body is released at rest and falls "
        "straight in.");

    if (ImGui::Button("Spawn", ImVec2(-1, 0))) app.spawnFromCamera();

    ImGui::SeparatorText("Exact placement");
    ImGui::TextDisabled("Position and velocity in metres and m/s.");
    ImGui::InputDouble("x", &state.spawnTemplate.position.x, 0.0, 0.0, "%.6g");
    ImGui::InputDouble("y", &state.spawnTemplate.position.y, 0.0, 0.0, "%.6g");
    ImGui::InputDouble("z", &state.spawnTemplate.position.z, 0.0, 0.0, "%.6g");
    ImGui::InputDouble("vx", &state.spawnTemplate.velocity.x, 0.0, 0.0, "%.6g");
    ImGui::InputDouble("vy", &state.spawnTemplate.velocity.y, 0.0, 0.0, "%.6g");
    ImGui::InputDouble("vz", &state.spawnTemplate.velocity.z, 0.0, 0.0, "%.6g");
    if (ImGui::Button("Spawn at these coordinates", ImVec2(-1, 0))) {
        app.spawnBody(state.spawnTemplate);
        app.setStatus("Spawned " + state.spawnTemplate.name + " at exact coordinates");
    }

    endPanelBody();
    ImGui::End();
}

void DebugUI::bodyLabels(engine::Application& app) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float width = viewport->Size.x;
    const float height = viewport->Size.y;
    if (width <= 0.0f || height <= 0.0f) return;

    // The same view-projection the renderer used this frame, so a label cannot
    // drift away from its body.
    const glm::mat4& viewProjection = app.renderer().lastViewProjection();
    const render::RenderSettings& settings = app.renderSettings();
    const glm::dvec3 cameraPosition = app.camera().position();

    ImDrawList* draw = ImGui::GetBackgroundDrawList();

    for (const sim::CelestialBody& body : app.system().bodies()) {
        const glm::dvec3 relative =
            render::Renderer::relativePosition(body, cameraPosition, settings.metresPerUnit);
        const double radius = render::Renderer::visualRadius(body, settings);

        // Anchor the label just above the body's drawn top.
        const glm::vec4 clip =
            viewProjection * glm::vec4(glm::vec3(relative) +
                                           glm::vec3(0.0f, static_cast<float>(radius), 0.0f),
                                       1.0f);
        if (clip.w <= 0.0f) continue;  // behind the camera

        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (ndc.x < -1.3f || ndc.x > 1.3f || ndc.y < -1.3f || ndc.y > 1.3f) continue;

        const ImVec2 screen(viewport->Pos.x + (ndc.x * 0.5f + 0.5f) * width,
                            viewport->Pos.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * height);

        const bool selected = body.id == app.state().selected;
        const ImU32 color = ImGui::GetColorU32(
            ImVec4(body.color.r * 0.55f + 0.45f, body.color.g * 0.55f + 0.45f,
                   body.color.b * 0.55f + 0.45f, selected ? 1.0f : 0.72f));

        const ImVec2 textSize = ImGui::CalcTextSize(body.name.c_str());
        const ImVec2 origin(screen.x - textSize.x * 0.5f, screen.y - textSize.y - 6.0f);
        // A dark outline, so names stay readable against a bright star.
        draw->AddText(ImVec2(origin.x + 1.0f, origin.y + 1.0f),
                      ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.85f)),
                      body.name.c_str());
        draw->AddText(origin, color, body.name.c_str());
    }
}

void DebugUI::helpOverlay(engine::Application& app) {
    placePanel(PanelSlot::FloatingLower, 400.0f);
    if (!ImGui::Begin("Controls and caveats", &showHelp_)) {
        ImGui::End();
        return;
    }

    ImGui::SeparatorText("Camera");
    ImGui::BulletText("Hold right mouse   look around");
    ImGui::BulletText("W A S D            move");
    ImGui::BulletText("Space / Ctrl       up / down");
    ImGui::BulletText("Shift / Alt        faster / slower");
    ImGui::BulletText("Scroll             fly speed, or orbit distance");
    ImGui::BulletText("C                  toggle orbit / free flight");
    ImGui::BulletText("F                  focus the selected body");

    ImGui::SeparatorText("Simulation");
    ImGui::BulletText("P                  pause / resume");
    ImGui::BulletText(".                  single step");
    ImGui::BulletText("[ ]                halve / double time scale");
    ImGui::BulletText("R                  reset the scene");
    ImGui::BulletText("N                  spawn a body ahead of the camera");
    ImGui::BulletText("Tab                cycle selection");
    ImGui::BulletText("Left click         select a body");

    ImGui::SeparatorText("View");
    ImGui::BulletText("G  grid    T  trails    V  velocity    B  r_s");
    ImGui::BulletText("F5 reload shaders   F12 screenshot");

    ImGui::SeparatorText("What this is, and is not");
    ImGui::TextWrapped(
        "The dynamics are Newtonian throughout: every body is accelerated by "
        "a = G m / r^2 summed over every other body, integrated with a fixed "
        "step. There are no relativistic corrections.\n\n"
        "The warped grid is a visualisation. It is displaced by the softened "
        "Newtonian potential and is a one-way read of the simulation: nothing "
        "in the physics reads the grid back. It is the familiar rubber-sheet "
        "analogy, not a picture of curved spacetime, and not a solution of the "
        "Einstein field equations.\n\n"
        "Compact objects here are Newtonian point masses. Their Schwarzschild "
        "radius is reported because it is a useful reference number, but no "
        "event horizon, lensing or time dilation is modelled.");

    ImGui::TextDisabled("Scene: %s", app.state().currentSceneKey.c_str());
    ImGui::End();
}

}  // namespace ui
