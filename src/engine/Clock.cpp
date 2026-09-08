#include "engine/Clock.h"

#include <GLFW/glfw3.h>

namespace engine {

void Clock::tick() {
    const double now = glfwGetTime();
    if (last_ < 0.0) {
        last_ = now;
        return;
    }
    delta_ = now - last_;
    last_ = now;
    if (delta_ > maxDelta_) delta_ = maxDelta_;
    elapsed_ += delta_;
    ++frames_;

    if (delta_ > 0.0) {
        const double instant = 1.0 / delta_;
        smoothedFps_ = smoothedFps_ <= 0.0 ? instant
                                           : smoothedFps_ * 0.92 + instant * 0.08;
    }
}

}  // namespace engine
