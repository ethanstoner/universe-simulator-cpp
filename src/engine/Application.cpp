#include "engine/Application.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "engine/Screenshot.h"
#include "render/MeshFactory.h"
#include "sim/Constants.h"

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
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);

    renderReady_ = flatShader_.loadFromFiles("shaders/flat.vert", "shaders/flat.frag");
    circleMesh_ = render::uploadMesh(render::makeCircle(64));
    outlineMesh_ = render::uploadMesh(render::makeBoxOutline());

    loadScene(options_.scene);
}

Application::~Application() = default;

void Application::loadScene(const std::string& name) {
    system_.clear();
    time_.reset();

    // M2 kinematics lab: uniform Earth-surface gravity, no pairwise attraction,
    // a box to bounce inside.
    sim::SimulationSettings& settings = system_.settings();
    settings.pairwiseGravityEnabled = false;
    settings.uniformGravity = sim::Vec3(0.0, -sim::constants::kEarthSurfaceGravity, 0.0);
    settings.integrator = sim::IntegratorType::VelocityVerlet;
    settings.bounds.enabled = true;
    settings.bounds.min = sim::Vec3(-16.0, 0.0, -4.0);
    settings.bounds.max = sim::Vec3(16.0, 18.0, 4.0);
    settings.bounds.restitution = 0.82;
    settings.bounds.friction = 0.02;
    settings.trailLength = 240;
    settings.collisionMode = sim::CollisionMode::Elastic;
    settings.collisionRestitution = 0.9;

    // Three balls dropped from different heights with different restitution is
    // the clearest way to see that the bounce is physics and not an animation.
    struct Drop {
        const char* name;
        double x;
        double y;
        double vx;
        double radius;
        glm::vec3 color;
    };
    const Drop drops[] = {
        {"ball-a", -9.0, 16.0, 2.4, 0.7, {0.95f, 0.45f, 0.30f}},
        {"ball-b", 0.0, 12.0, -1.2, 0.5, {0.40f, 0.75f, 0.98f}},
        {"ball-c", 8.0, 17.0, -3.0, 0.9, {0.85f, 0.80f, 0.35f}},
    };
    for (const Drop& drop : drops) {
        sim::CelestialBody body;
        body.name = drop.name;
        body.mass = 1.0;
        body.radius = drop.radius;
        body.position = sim::Vec3(drop.x, drop.y, 0.0);
        body.velocity = sim::Vec3(drop.vx, 0.0, 0.0);
        body.color = drop.color;
        system_.add(body);
    }

    time_.fixedDt = 1.0 / 240.0;
    time_.timeScale = 1.0;
    (void)name;
}

int Application::run() {
    // Headless capture needs deterministic content, so advance the simulation by
    // a requested amount of *simulated* time before the first frame instead of
    // hoping the wall clock cooperates.
    if (options_.warmupSimSeconds > 0.0) {
        const int steps = static_cast<int>(options_.warmupSimSeconds / time_.fixedDt);
        for (int i = 0; i < steps; ++i) system_.step(time_.fixedDt);
    }

    while (!window_->shouldClose()) {
        clock_.tick();
        input_.newFrame();

        processInput();
        advanceSimulation(clock_.deltaSeconds());
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
    if (input_.keyPressed(GLFW_KEY_F5)) {
        if (flatShader_.reload()) std::printf("[shader] reloaded\n");
    }
    if (input_.keyPressed(GLFW_KEY_SPACE)) time_.paused = !time_.paused;
    if (input_.keyPressed(GLFW_KEY_PERIOD)) time_.singleStepRequested = true;
    if (input_.keyPressed(GLFW_KEY_R)) loadScene(options_.scene);
    if (input_.keyPressed(GLFW_KEY_LEFT_BRACKET)) time_.timeScale *= 0.5;
    if (input_.keyPressed(GLFW_KEY_RIGHT_BRACKET)) time_.timeScale *= 2.0;
}

void Application::advanceSimulation(double realDelta) {
    const int steps = time_.stepsForFrame(realDelta);
    for (int i = 0; i < steps; ++i) system_.step(time_.fixedDt);
}

void Application::render() {
    glViewport(0, 0, window_->framebufferWidth(), window_->framebufferHeight());
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!renderReady_) return;
    renderKinematicsLab();
}

void Application::renderKinematicsLab() {
    // Orthographic so this reads as the flat 2D demonstration it is. The view
    // volume is fitted to the bounds box with a margin, then widened to the
    // window's aspect ratio so nothing is squashed when the window is resized.
    const sim::Bounds& bounds = system_.settings().bounds;
    const float margin = 2.0f;
    const float left = static_cast<float>(bounds.min.x) - margin;
    const float right = static_cast<float>(bounds.max.x) + margin;
    const float bottom = static_cast<float>(bounds.min.y) - margin;
    const float top = static_cast<float>(bounds.max.y) + margin;

    float halfWidth = (right - left) * 0.5f;
    float halfHeight = (top - bottom) * 0.5f;
    const float aspect = window_->aspect();
    if (halfWidth / halfHeight < aspect) {
        halfWidth = halfHeight * aspect;
    } else {
        halfHeight = halfWidth / aspect;
    }
    const glm::vec2 centre{(left + right) * 0.5f, (bottom + top) * 0.5f};

    const glm::mat4 projection =
        glm::ortho(centre.x - halfWidth, centre.x + halfWidth, centre.y - halfHeight,
                   centre.y + halfHeight, -10.0f, 10.0f);

    flatShader_.bind();
    flatShader_.setMat4("uViewProjection", projection);

    // The bounds box, drawn as a wireframe so the floor and walls are visible.
    {
        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3((bounds.min.x + bounds.max.x) * 0.5,
                                                (bounds.min.y + bounds.max.y) * 0.5, 0.0));
        model = glm::scale(model, glm::vec3(bounds.max.x - bounds.min.x,
                                            bounds.max.y - bounds.min.y, 1.0f));
        flatShader_.setMat4("uModel", model);
        flatShader_.setVec4("uColor", glm::vec4(0.25f, 0.30f, 0.42f, 1.0f));
        outlineMesh_.draw(render::DrawMode::Lines);
    }

    // Trails, drawn as a strip of small dots so no extra renderer is needed yet.
    for (const sim::CelestialBody& body : system_.bodies()) {
        if (body.trail.size() < 2) continue;
        std::size_t index = 0;
        for (const sim::Vec3& point : body.trail) {
            ++index;
            if (index % 4 != 0) continue;  // thin the trail out
            const float fade = static_cast<float>(index) /
                               static_cast<float>(body.trail.size());
            glm::mat4 model(1.0f);
            model = glm::translate(model, glm::vec3(point.x, point.y, -0.5f));
            model = glm::scale(model, glm::vec3(0.08f));
            flatShader_.setMat4("uModel", model);
            flatShader_.setVec4("uColor", glm::vec4(body.color, 0.10f + 0.35f * fade));
            circleMesh_.draw();
        }
    }

    for (const sim::CelestialBody& body : system_.bodies()) {
        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(body.position.x, body.position.y, 0.0f));
        model = glm::scale(model, glm::vec3(static_cast<float>(body.radius)));
        flatShader_.setMat4("uModel", model);
        flatShader_.setVec4("uColor", glm::vec4(body.color, 1.0f));
        circleMesh_.draw();
    }
}

}  // namespace engine
