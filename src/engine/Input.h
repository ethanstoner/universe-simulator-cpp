#pragma once

#include <array>
#include <glm/vec2.hpp>

struct GLFWwindow;

namespace engine {

// Polled input state with edge detection. Held against GLFW's key codes
// directly; nothing here knows what the keys mean.
class Input {
public:
    void attach(GLFWwindow* window);

    // Call once per frame, before update/render.
    void newFrame();

    bool keyDown(int key) const;
    bool keyPressed(int key) const;   // went down this frame
    bool keyReleased(int key) const;  // went up this frame

    bool mouseDown(int button) const;
    bool mousePressed(int button) const;
    bool mouseReleased(int button) const;

    glm::vec2 mousePosition() const { return mousePos_; }
    glm::vec2 mouseDelta() const { return mouseDelta_; }
    float scrollDelta() const { return scroll_; }

    // When the mouse is captured the cursor is hidden and locked to the window,
    // which is what free-look flying wants.
    void setMouseCaptured(bool captured);
    bool mouseCaptured() const { return captured_; }

private:
    // GLFW_KEY_LAST is 348; polling past it makes glfwGetKey raise
    // GLFW_INVALID_ENUM once per key per frame.
    static constexpr int kMaxKeys = 349;
    static constexpr int kFirstKey = 32;  // GLFW_KEY_SPACE, the lowest valid code
    static constexpr int kMaxButtons = 8;

    static void scrollCallback(GLFWwindow* window, double x, double y);
    static Input* instance_;  // the window user pointer is owned by Window

    GLFWwindow* window_ = nullptr;
    std::array<bool, kMaxKeys> keys_{};
    std::array<bool, kMaxKeys> prevKeys_{};
    std::array<bool, kMaxButtons> buttons_{};
    std::array<bool, kMaxButtons> prevButtons_{};
    glm::vec2 mousePos_{0.0f};
    glm::vec2 mouseDelta_{0.0f};
    float scroll_ = 0.0f;
    float scrollAccum_ = 0.0f;
    bool captured_ = false;
    bool firstMouse_ = true;
};

}  // namespace engine
