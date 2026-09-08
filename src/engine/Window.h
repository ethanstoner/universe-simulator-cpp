#pragma once

#include <string>

struct GLFWwindow;

namespace engine {

struct WindowConfig {
    int width = 1600;
    int height = 900;
    std::string title = "gravitysim";
    bool visible = true;   // headless screenshot runs create a hidden window
    bool vsync = true;
    int msaaSamples = 4;
};

// Owns the GLFW window, the OpenGL 3.3 core context and the GLAD function
// pointers loaded into it. Exactly one of these should exist at a time.
class Window {
public:
    explicit Window(const WindowConfig& config);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool shouldClose() const;
    void requestClose();
    void swapBuffers();
    static void pollEvents();

    GLFWwindow* handle() const { return window_; }
    int framebufferWidth() const { return fbWidth_; }
    int framebufferHeight() const { return fbHeight_; }
    float aspect() const;

    void setVsync(bool enabled);
    void setTitle(const std::string& title);

private:
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);

    GLFWwindow* window_ = nullptr;
    int fbWidth_ = 0;
    int fbHeight_ = 0;
};

}  // namespace engine
