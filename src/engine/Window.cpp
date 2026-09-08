#include "engine/Window.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <stdexcept>

namespace engine {
namespace {

int g_glfwRefCount = 0;

void glfwErrorCallback(int code, const char* description) {
    std::fprintf(stderr, "[glfw] error %d: %s\n", code, description);
}

}  // namespace

Window::Window(const WindowConfig& config) {
    if (g_glfwRefCount++ == 0) {
        glfwSetErrorCallback(glfwErrorCallback);
        if (!glfwInit()) {
            g_glfwRefCount = 0;
            throw std::runtime_error("glfwInit failed");
        }
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, config.msaaSamples);
    glfwWindowHint(GLFW_VISIBLE, config.visible ? GLFW_TRUE : GLFW_FALSE);

    window_ = glfwCreateWindow(config.width, config.height, config.title.c_str(),
                               nullptr, nullptr);
    if (!window_) {
        throw std::runtime_error("glfwCreateWindow failed (no OpenGL 3.3 core context?)");
    }

    glfwMakeContextCurrent(window_);
    if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress))) {
        throw std::runtime_error("gladLoadGL failed");
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferSizeCallback);
    glfwGetFramebufferSize(window_, &fbWidth_, &fbHeight_);
    setVsync(config.vsync);

    std::printf("[gl] %s | %s | GLSL %s\n",
                glGetString(GL_RENDERER), glGetString(GL_VERSION),
                glGetString(GL_SHADING_LANGUAGE_VERSION));
}

Window::~Window() {
    if (window_) glfwDestroyWindow(window_);
    if (--g_glfwRefCount == 0) glfwTerminate();
}

void Window::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!self) return;
    self->fbWidth_ = width;
    self->fbHeight_ = height;
    glViewport(0, 0, width, height);
}

bool Window::shouldClose() const { return glfwWindowShouldClose(window_) != 0; }
void Window::requestClose() { glfwSetWindowShouldClose(window_, GLFW_TRUE); }
void Window::swapBuffers() { glfwSwapBuffers(window_); }
void Window::pollEvents() { glfwPollEvents(); }

float Window::aspect() const {
    if (fbHeight_ <= 0) return 1.0f;
    return static_cast<float>(fbWidth_) / static_cast<float>(fbHeight_);
}

void Window::setVsync(bool enabled) { glfwSwapInterval(enabled ? 1 : 0); }

void Window::setTitle(const std::string& title) {
    glfwSetWindowTitle(window_, title.c_str());
}

}  // namespace engine
