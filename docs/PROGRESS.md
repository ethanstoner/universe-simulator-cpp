# Progress

Milestones are only marked COMPLETE after the code has been built, run, and its
behaviour checked -- for rendering work that means an actual captured frame was
inspected, not that the draw call was issued.

---

## M1 -- OpenGL window and render loop

Status: **COMPLETE**

Implemented:
- CMake 3.24 / C++20 project, Ninja + MinGW g++ 16.2.
- Dependencies: GLFW 3.4, GLM 1.0.1 and Dear ImGui v1.91.5 via `FetchContent`;
  GLAD vendored under `external/glad` (glad2, `gl:core=3.3`, no extensions).
- `engine::Window` (GLFW + GL 3.3 core + GLAD load + resize callback),
  `engine::Clock` (frame delta, clamped, smoothed FPS), `engine::Input`
  (polled state with press/release edges, scroll, cursor capture),
  `engine::Application` (the `processInput / update / render / swap / poll`
  loop), `engine::resolveAsset`, `engine::captureFramebufferToPng`.
- A dependency-free test runner (`tests/TestFramework.h`) wired to CTest.
- SI constants and human-readable unit formatting (`sim/Units`).

Build:
```
cmake -S . -B build -G Ninja
cmake --build build
```
Clean, no warnings-as-errors configured yet.

Run:
```
build/bin/gravitysim.exe
build/bin/gravitysim.exe --screenshot docs/images/m1_window.png --frame 5
```

Tests: `build/bin/gravsim_tests.exe` -- 6 passed, 0 failed (unit formatting).

Observed behaviour:
- Context reported as `NVIDIA GeForce RTX 4090/PCIe/SSE2 | 3.3.0 NVIDIA 610.88 |
  GLSL 3.30`.
- `docs/images/m1_window.png` captured and visually inspected: uniform
  `#050508` field at the requested 640x360, which matches the clear colour.
  This also proves the hand-written PNG encoder is producing valid files.

Problems found and fixed:
1. **POST_BUILD asset staging failed.** CMake wraps custom commands in a nested
   `cmd /C "cd /D <path> && ..."`. The repository path contains `AI & Media`;
   `cmd` re-parsed the `&` as a command separator and reported
   `'Media' is not recognized as an internal or external command`. Replaced the
   copy step with `engine::resolveAsset`, which searches cwd, the exe directory,
   and the configure-time source directory.
2. **`glfwGetKey` spammed `GLFW_INVALID_ENUM`.** `Input::newFrame` polled key
   codes up to 511 but `GLFW_KEY_LAST` is 348, producing ~160 error callbacks
   per frame. Key range now stops at `GLFW_KEY_LAST`.
3. **Unit formatter chose megametres** for Earth's radius. `6371000 m` rendered
   as `6.371 Mm`; astronomy uses km for planetary radii. The `Mm` branch was
   removed so km runs all the way to the AU threshold.

Known issues: none outstanding.

Next: M2 -- triangulated circle mesh, fixed-timestep kinematics, bouncing.
