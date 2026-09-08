#pragma once

namespace engine {

// Wall-clock frame timing. Deliberately separate from simulated time: the
// simulation advances on a fixed physics step driven by an accumulator (see
// sim/Integrator.h), so rendering rate never changes physics behaviour.
class Clock {
public:
    void tick();

    double deltaSeconds() const { return delta_; }
    double elapsedSeconds() const { return elapsed_; }
    double fps() const { return smoothedFps_; }
    unsigned long long frameCount() const { return frames_; }

    // Frame deltas are clamped so that a stall (debugger break, window drag)
    // cannot inject a huge dt into the physics accumulator.
    void setMaxDelta(double seconds) { maxDelta_ = seconds; }

private:
    double last_ = -1.0;
    double delta_ = 0.0;
    double elapsed_ = 0.0;
    double maxDelta_ = 0.25;
    double smoothedFps_ = 0.0;
    unsigned long long frames_ = 0;
};

}  // namespace engine
