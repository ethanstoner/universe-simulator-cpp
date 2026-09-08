#include "engine/Application.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "engine/Screenshot.h"

namespace engine {

void AppOptions::printUsage() {
    std::printf(
        "gravitysim -- interactive Newtonian gravity simulator\n"
        "\n"
        "  --width N --height N     framebuffer size (default 1600x900)\n"
        "  --scene NAME             load a scene preset at startup\n"
        "  --list-scenes            print available scene presets and exit\n"
        "  --screenshot PATH        render offscreen, write a PNG, exit\n"
        "  --frame N                frame to capture on (default 60)\n"
        "  --warmup SECONDS         simulated seconds to advance before capture\n"
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
    WindowConfig config;
    config.width = options.width;
    config.height = options.height;
    config.title = "gravitysim";
    config.visible = !options.hidden;
    config.vsync = options.vsync;
    window_ = std::make_unique<Window>(config);
    input_.attach(window_->handle());

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
}

Application::~Application() = default;

int Application::run() {
    while (!window_->shouldClose()) {
        clock_.tick();
        input_.newFrame();

        processInput();
        update(clock_.deltaSeconds());
        render();

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
    if (input_.keyPressed(GLFW_KEY_ESCAPE)) window_->requestClose();
    if (input_.keyPressed(GLFW_KEY_F12)) {
        captureFramebufferToPng(window_->framebufferWidth(),
                                window_->framebufferHeight(), "screenshot.png");
    }
}

void Application::update(double /*dt*/) {}

void Application::render() {
    glViewport(0, 0, window_->framebufferWidth(), window_->framebufferHeight());
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

}  // namespace engine
