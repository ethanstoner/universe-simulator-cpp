#pragma once

struct GLFWwindow;

namespace engine {
class Application;
}

namespace ui {

// All ImGui panels. Reads and writes the Application's state directly rather
// than mirroring it, so there is exactly one copy of every setting.
class DebugUI {
public:
    void initialize(GLFWwindow* window);
    void shutdown();

    void beginFrame();
    void build(engine::Application& app);
    void endFrame();

    // True when ImGui has focus, so the application can stop stealing keys or
    // treating a click on a panel as a click in the viewport.
    bool wantsKeyboard() const;
    bool wantsMouse() const;

private:
    void simulationPanel(engine::Application& app);
    void systemPanel(engine::Application& app);
    void renderingPanel(engine::Application& app);
    void bodiesPanel(engine::Application& app);
    void inspectorPanel(engine::Application& app);
    void spawnPanel(engine::Application& app);
    void scenePanel(engine::Application& app);
    void helpOverlay(engine::Application& app);
    // Body names drawn in screen space, projected from the same
    // camera-relative positions the renderer uses.
    void bodyLabels(engine::Application& app);

    bool initialized_ = false;
    bool showSimulation_ = true;
    bool showSystem_ = true;
    bool showRendering_ = true;
    bool showBodies_ = true;
    bool showSpawn_ = false;
    bool showScenes_ = true;
    bool showHelp_ = false;
};

}  // namespace ui
