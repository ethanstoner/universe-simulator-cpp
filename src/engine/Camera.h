#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace engine {

class Input;

enum class CameraMode {
    Fly,    // free flight: WASD + mouse look
    Orbit,  // locked to a focus point, dragging swings around it
};

// Perspective camera positioned in *world units*, not metres. The conversion
// from metres happens once, in the renderer, using SceneView::metresPerUnit.
//
// The position is a dvec3 even though it is eventually handed to the GPU as
// floats: the whole point of the camera-relative pipeline is that the
// subtraction (body - camera) happens in double, so that a body 1e11 m away
// still lands on an exact float when it is near the camera.
class Camera {
public:
    void update(const Input& input, double deltaSeconds, bool acceptInput);

    glm::mat4 view() const;         // eye fixed at the origin, see position()
    glm::mat4 projection(float aspect) const;
    glm::mat4 viewProjection(float aspect) const { return projection(aspect) * view(); }

    glm::dvec3 position() const { return position_; }
    void setPosition(const glm::dvec3& position) { position_ = position; }

    glm::vec3 forward() const;
    glm::vec3 right() const;
    glm::vec3 up() const;

    // Points the camera at `target` (world units) from `distance` away, using
    // the current yaw/pitch as the approach direction.
    void frame(const glm::dvec3& target, double distance);

    void setMode(CameraMode mode) { mode_ = mode; }
    CameraMode mode() const { return mode_; }

    // In Orbit mode the camera is recomputed every frame from this point, which
    // the renderer updates so it tracks a moving body.
    void setOrbitTarget(const glm::dvec3& target) { orbitTarget_ = target; }
    glm::dvec3 orbitTarget() const { return orbitTarget_; }
    double orbitDistance() const { return orbitDistance_; }
    void setOrbitDistance(double distance);

    float fovDegrees = 55.0f;
    float nearPlane = 0.01f;
    float farPlane = 20000.0f;
    float moveSpeed = 18.0f;        // world units per second
    float mouseSensitivity = 0.12f; // degrees per pixel
    float yaw = -90.0f;             // degrees; -90 looks down -Z
    float pitch = -22.0f;

    // Converts a viewport pixel into a ray in world-unit space, with the origin
    // at the camera. Used for click-to-select and click-to-spawn.
    void screenRay(float pixelX, float pixelY, int width, int height,
                   glm::vec3& outDirection) const;

private:
    void applyLook(const Input& input, bool acceptInput);

    glm::dvec3 position_{0.0, 12.0, 45.0};
    glm::dvec3 orbitTarget_{0.0};
    double orbitDistance_ = 45.0;
    CameraMode mode_ = CameraMode::Fly;
};

}  // namespace engine
