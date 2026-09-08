#include "engine/Camera.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

#include "engine/Input.h"

namespace engine {
namespace {
constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
}

glm::vec3 Camera::forward() const {
    const float yawRad = yaw * kDegToRad;
    const float pitchRad = pitch * kDegToRad;
    return glm::normalize(glm::vec3(std::cos(yawRad) * std::cos(pitchRad),
                                    std::sin(pitchRad),
                                    std::sin(yawRad) * std::cos(pitchRad)));
}

glm::vec3 Camera::right() const {
    return glm::normalize(glm::cross(forward(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::vec3 Camera::up() const { return glm::normalize(glm::cross(right(), forward())); }

glm::mat4 Camera::view() const {
    // The eye sits at the origin. Everything drawn has already had the camera
    // position subtracted in double precision, so this matrix carries no large
    // translation and cannot lose precision to one.
    return glm::lookAt(glm::vec3(0.0f), forward(), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::projection(float aspect) const {
    if (aspect <= 0.0f) aspect = 1.0f;
    return glm::perspective(glm::radians(fovDegrees), aspect, nearPlane, farPlane);
}

void Camera::setOrbitDistance(double distance) {
    orbitDistance_ = std::clamp(distance, 0.02, 1.0e6);
}

void Camera::frame(const glm::dvec3& target, double distance) {
    orbitTarget_ = target;
    setOrbitDistance(distance);
    position_ = target - glm::dvec3(forward()) * orbitDistance_;
}

void Camera::applyLook(const Input& input, bool acceptInput) {
    if (!acceptInput || !input.mouseCaptured()) return;
    const glm::vec2 delta = input.mouseDelta();
    yaw += delta.x * mouseSensitivity;
    pitch -= delta.y * mouseSensitivity;
    // Clamped just short of straight up/down: at exactly +-90 the forward
    // vector becomes parallel to the world up and the basis degenerates.
    pitch = std::clamp(pitch, -89.5f, 89.5f);
}

void Camera::update(const Input& input, double deltaSeconds, bool acceptInput) {
    applyLook(input, acceptInput);

    if (mode_ == CameraMode::Orbit) {
        if (acceptInput) {
            const float scroll = input.scrollDelta();
            if (scroll != 0.0f) {
                // Multiplicative zoom so the step feels the same at every scale.
                setOrbitDistance(orbitDistance_ * std::pow(0.85, scroll));
            }
        }
        position_ = orbitTarget_ - glm::dvec3(forward()) * orbitDistance_;
        return;
    }

    if (!acceptInput) return;

    // Scroll adjusts fly speed rather than FOV: at astronomical scales the
    // useful range of speeds spans several orders of magnitude, and a zoom that
    // narrows the FOV cannot cross that.
    const float scroll = input.scrollDelta();
    if (scroll != 0.0f) {
        moveSpeed = std::clamp(moveSpeed * std::pow(1.25f, scroll), 0.01f, 100000.0f);
    }

    float speed = moveSpeed;
    if (input.keyDown(GLFW_KEY_LEFT_SHIFT)) speed *= 6.0f;
    if (input.keyDown(GLFW_KEY_LEFT_ALT)) speed *= 0.15f;

    glm::vec3 movement(0.0f);
    if (input.keyDown(GLFW_KEY_W)) movement += forward();
    if (input.keyDown(GLFW_KEY_S)) movement -= forward();
    if (input.keyDown(GLFW_KEY_D)) movement += right();
    if (input.keyDown(GLFW_KEY_A)) movement -= right();
    if (input.keyDown(GLFW_KEY_SPACE)) movement += glm::vec3(0.0f, 1.0f, 0.0f);
    if (input.keyDown(GLFW_KEY_LEFT_CONTROL)) movement -= glm::vec3(0.0f, 1.0f, 0.0f);

    if (glm::dot(movement, movement) > 0.0f) {
        position_ += glm::dvec3(glm::normalize(movement)) *
                     (static_cast<double>(speed) * deltaSeconds);
    }
}

void Camera::screenRay(float pixelX, float pixelY, int width, int height,
                       glm::vec3& outDirection) const {
    if (width <= 0 || height <= 0) {
        outDirection = forward();
        return;
    }
    // Pixel -> normalised device coordinates. GLFW's cursor origin is the top
    // left, NDC's is the centre with +Y up, hence the flip.
    const float ndcX = 2.0f * pixelX / static_cast<float>(width) - 1.0f;
    const float ndcY = 1.0f - 2.0f * pixelY / static_cast<float>(height);

    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float tanHalfFov = std::tan(glm::radians(fovDegrees) * 0.5f);

    outDirection = glm::normalize(forward() + right() * (ndcX * tanHalfFov * aspect) +
                                  up() * (ndcY * tanHalfFov));
}

}  // namespace engine
