#include "engine/TimeControl.h"

#include <algorithm>

namespace engine {

void TimeControl::reset() {
    accumulator = 0.0;
    stepsLastFrame = 0;
    budgetExceeded = false;
    stepsPerSecond = 0.0;
    singleStepRequested = false;
}

int TimeControl::stepsForFrame(double realDeltaSeconds) {
    budgetExceeded = false;

    if (paused) {
        accumulator = 0.0;
        if (singleStepRequested) {
            singleStepRequested = false;
            stepsLastFrame = 1;
            return 1;
        }
        stepsLastFrame = 0;
        stepsPerSecond = 0.0;
        return 0;
    }
    singleStepRequested = false;

    if (fixedDt <= 0.0 || realDeltaSeconds <= 0.0) {
        stepsLastFrame = 0;
        return 0;
    }

    accumulator += realDeltaSeconds * timeScale;

    int steps = static_cast<int>(accumulator / fixedDt);
    if (steps > maxStepsPerFrame) {
        steps = maxStepsPerFrame;
        budgetExceeded = true;
        // Drop the backlog rather than carrying it forward, which would make
        // the simulation try to catch up forever after one slow frame.
        accumulator = 0.0;
    } else {
        accumulator -= steps * fixedDt;
    }

    stepsLastFrame = steps;
    if (realDeltaSeconds > 0.0) {
        const double instant = steps / realDeltaSeconds;
        stepsPerSecond = stepsPerSecond <= 0.0 ? instant
                                               : stepsPerSecond * 0.9 + instant * 0.1;
    }
    return steps;
}

}  // namespace engine
