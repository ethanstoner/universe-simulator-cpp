#pragma once

namespace engine {

// Decouples simulated time from frame rate.
//
// The physics step is *fixed*. Time acceleration multiplies how much simulated
// time a frame is asked to cover, which turns into more steps -- never into a
// larger step. Enlarging dt with timeScale is the classic way to make a stable
// orbit fly apart at 1000x while looking fine at 1x.
struct TimeControl {
    double fixedDt = 1.0 / 120.0;  // seconds of simulated time per physics step
    double timeScale = 1.0;        // simulated seconds per real second
    bool paused = false;

    // Guards against a spiral of death: if a frame's work cannot be finished in
    // time, the simulation runs slower than requested rather than the process
    // locking up. The UI surfaces `budgetExceeded` so this is visible instead
    // of being mistaken for correct behaviour.
    int maxStepsPerFrame = 4000;

    double accumulator = 0.0;
    int stepsLastFrame = 0;
    bool budgetExceeded = false;
    double stepsPerSecond = 0.0;

    // Set by the UI to advance exactly one step while paused.
    bool singleStepRequested = false;

    // Returns how many fixed steps should run this frame and consumes them from
    // the accumulator.
    int stepsForFrame(double realDeltaSeconds);

    void reset();
};

}  // namespace engine
