# Third-party components

| Component | Licence | Source | How it is used |
| --- | --- | --- | --- |
| GLFW 3.4 | zlib/libpng | https://github.com/glfw/glfw | Window, input and OpenGL context. Fetched at configure time. |
| GLM 1.0.1 | MIT | https://github.com/g-truc/glm | Vector and matrix maths. Fetched at configure time, header only. |
| Dear ImGui | MIT | https://github.com/ocornut/imgui | The control panels. Fetched at configure time. |
| glad 2 | MIT | https://github.com/Dav1dde/glad | OpenGL function loader. The generated output is vendored under `external/glad`. |

GLFW, GLM and Dear ImGui are downloaded by CMake's `FetchContent` and are **not**
redistributed in this repository. Only the generated glad loader is vendored, so
that the project builds without needing the glad generator or a network
connection for that step.

Astronomical constants (masses, mean radii, semi-major axes) are IAU and NASA
fact-sheet values. They are facts, not a copyrightable dataset, and are listed
with their sources in `docs/PHYSICS.md`.
