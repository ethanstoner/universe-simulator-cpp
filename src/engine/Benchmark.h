#pragma once

namespace engine {

// Measures direct summation against Barnes-Hut across a range of body counts
// and prints a table. Invoked with --benchmark; needs no window, but lives in
// engine/ rather than sim/ because it is a tool rather than physics.
//
// Reports accuracy alongside speed on purpose: a solver that is faster and
// wrong is not faster.
int runBenchmark(int maxBodies);

}  // namespace engine
