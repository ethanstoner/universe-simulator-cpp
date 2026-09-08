#include "engine/Input.h"

#include <GLFW/glfw3.h>

namespace engine {

Input* Input::instance_ = nullptr;

void Input::attach(GLFWwindow* window) {
    window_ = window;
    instance_ = this;
    // Installed before ImGui's backend so that ImGui chains to this callback
    // rather than replacing it.
    glfwSetScrollCallback(window, scrollCallback);
}

void Input::scrollCallback(GLFWwindow*, double, double y) {
    if (instance_) instance_->scrollAccum_ += static_cast<float>(y);
}

void Input::newFrame() {
    if (!window_) return;

    prevKeys_ = keys_;
    prevButtons_ = buttons_;

    for (int key = kFirstKey; key < kMaxKeys; ++key) {
        keys_[key] = glfwGetKey(window_, key) == GLFW_PRESS;
    }
    for (int button = 0; button < kMaxButtons; ++button) {
        buttons_[button] = glfwGetMouseButton(window_, button) == GLFW_PRESS;
    }

    double x = 0.0, y = 0.0;
    glfwGetCursorPos(window_, &x, &y);
    const glm::vec2 pos{static_cast<float>(x), static_cast<float>(y)};
    if (firstMouse_) {
        mousePos_ = pos;
        firstMouse_ = false;
    }
    mouseDelta_ = pos - mousePos_;
    mousePos_ = pos;

    scroll_ = scrollAccum_;
    scrollAccum_ = 0.0f;
}

bool Input::keyDown(int key) const {
    return key >= 0 && key < kMaxKeys && keys_[key];
}
bool Input::keyPressed(int key) const {
    return key >= 0 && key < kMaxKeys && keys_[key] && !prevKeys_[key];
}
bool Input::keyReleased(int key) const {
    return key >= 0 && key < kMaxKeys && !keys_[key] && prevKeys_[key];
}
bool Input::mouseDown(int button) const {
    return button >= 0 && button < kMaxButtons && buttons_[button];
}
bool Input::mousePressed(int button) const {
    return button >= 0 && button < kMaxButtons && buttons_[button] && !prevButtons_[button];
}
bool Input::mouseReleased(int button) const {
    return button >= 0 && button < kMaxButtons && !buttons_[button] && prevButtons_[button];
}

void Input::setMouseCaptured(bool captured) {
    if (!window_ || captured == captured_) return;
    captured_ = captured;
    glfwSetInputMode(window_, GLFW_CURSOR,
                     captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    firstMouse_ = true;  // avoid a huge delta on the frame the mode changes
}

}  // namespace engine
