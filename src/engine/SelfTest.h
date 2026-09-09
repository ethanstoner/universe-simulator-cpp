#pragma once

namespace engine {

class Application;

// Exercises every state transition the UI can trigger, in a real application
// instance with a real GL context, and checks the invariants afterwards.
//
// ImGui itself cannot be driven programmatically here, so this deliberately
// calls the same Application methods the panels call -- loadScene, spawnBody,
// spawnFromCamera, deleteBody, focusOn, resetRenderDefaults and so on. That
// covers the logic behind the buttons even though it does not cover the click
// that reaches them.
//
// Returns 0 when every check passed.
int runSelfTest(Application& app);

}  // namespace engine
