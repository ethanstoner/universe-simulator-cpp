#pragma once

#include <memory>
#include <string>

#include "engine/Clock.h"
#include "engine/Input.h"
#include "engine/Window.h"

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
    std::string scene;               // preset to load at startup
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
    void processInput();
    void update(double dt);
    void render();

    AppOptions options_;
    std::unique_ptr<Window> window_;
    Input input_;
    Clock clock_;
    bool captured_ = false;
};

}  // namespace engine
