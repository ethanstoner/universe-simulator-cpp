#pragma once

#include <memory>
#include <string>

#include "engine/Clock.h"
#include "engine/Input.h"
#include "engine/TimeControl.h"
#include "engine/Window.h"
#include "render/Mesh.h"
#include "render/Shader.h"
#include "sim/GravitySystem.h"

namespace engine {

// Command line surface. The headless screenshot options exist so that every
// rendering milestone can be verified from a build script rather than by
// eyeballing a live window.
struct AppOptions {
    int width = 1600;
    int height = 900;
    bool hidden = false;             // create the window but never show it
    std::string screenshotPath;      // if set: render, capture, exit
    int screenshotFrame = 60;        // which frame to capture on
    double warmupSimSeconds = 0.0;   // advance the simulation before capturing
    std::string scene = "bounce";    // preset to load at startup
    bool listScenes = false;
    bool vsync = true;

    static AppOptions parse(int argc, char** argv);
    static void printUsage();
};

class Application {
public:
    explicit Application(const AppOptions& options);
    ~Application();

    int run();

private:
    void loadScene(const std::string& name);
    void processInput();
    void advanceSimulation(double realDelta);
    void render();
    void renderKinematicsLab();

    AppOptions options_;
    std::unique_ptr<Window> window_;
    Input input_;
    Clock clock_;
    TimeControl time_;

    sim::GravitySystem system_;

    render::Shader flatShader_;
    render::Mesh circleMesh_;
    render::Mesh outlineMesh_;
    bool renderReady_ = false;
};

}  // namespace engine
