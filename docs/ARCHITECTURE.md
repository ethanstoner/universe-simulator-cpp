# Architecture

## Layering rule

```
  sim/      pure C++20 + glm. No OpenGL, no GLFW, no ImGui, no I/O beyond files.
    ^
    |  (one-way dependency)
    |
  engine/   window, input, timing, camera, asset lookup, screenshots
  render/   shaders, meshes, renderers
  ui/       ImGui panels
```

`sim/` is built as a separate static library (`gravsim`) and the unit tests link
against **only** that library. If a physics change cannot be tested without
opening a window, the layering has been violated.

## Modules

| Path | Responsibility |
| --- | --- |
| `src/sim/Constants.h` | SI physical constants (G, c, AU, solar mass, ...) |
| `src/sim/Vec.h` | `Vec3` alias for `glm::dvec3` plus small helpers |
| `src/sim/CelestialBody.*` | One body: id, name, state vectors, mass, physical vs render radius |
| `src/sim/GravitySystem.*` | Owns the body list; pairwise gravity; stepping; collisions |
| `src/sim/Integrator.*` | Euler, symplectic Euler, velocity Verlet, RK4 |
| `src/sim/Collisions.*` | Ignore / elastic / merge response |
| `src/sim/Diagnostics.*` | Kinetic + potential energy, momentum, angular momentum, drift |
| `src/sim/OrbitMath.*` | Circular orbit velocity, Schwarzschild radius, orbital elements |
| `src/sim/SceneLibrary.*` | Built-in scene presets |
| `src/sim/SceneConfig.*` | JSON <-> scene loading |
| `src/sim/Json.*` | Small dependency-free JSON parser/writer |
| `src/sim/Units.*` | Human-readable formatting (m -> AU, kg -> M_sun, s -> yr) |
| `src/engine/*` | Window, Input, Clock, Camera, AssetPaths, Screenshot, Application |
| `src/render/*` | Shader, Mesh, MeshFactory, Renderer, Grid/Trail/Line renderers |
| `src/ui/DebugUI.*` | The ImGui control panel |

## Units

The simulation core is **strictly SI**: metres, kilograms, seconds, and
therefore m/s, m/s^2, joules, kg m/s. `G = 6.67430e-11 m^3 kg^-1 s^-2`.

Positions, velocities and accelerations are `double`. This is not optional:
Earth's orbital radius is ~1.5e11 m and `float` has ~7 significant decimal
digits, which would quantise Earth's position to steps of roughly 10 000 km.

## Scales

Three independent scale factors exist and must never be confused:

| Name | Meaning | Used by |
| --- | --- | --- |
| physical radius | the body's true radius, metres | collision/merge tests |
| `renderScale` | metres -> world units for the GPU | position transform only |
| `bodyVisualScale` | per-body exaggeration of drawn size | model matrix only |

**Gravity uses mass and position only.** Neither `renderScale` nor
`bodyVisualScale` is readable from `sim/`; they live in the renderer. This is
enforced structurally: `sim/CelestialBody` stores `renderRadiusScale` purely as
a display hint and no code in `sim/` reads it for physics.

## Timestep

Rendering rate never influences physics. `Application` accumulates real frame
time, multiplies by `timeScale`, and hands whole fixed steps to the integrator:

```
accumulator += frameDelta * timeScale
while (accumulator >= fixedDt && steps < maxStepsPerFrame) {
    system.step(fixedDt);
    accumulator -= fixedDt;
}
```

Raising `timeScale` therefore performs *more steps*, not *bigger steps*. There
is a `maxStepsPerFrame` cap so an extreme time scale degrades into slow motion
rather than freezing the process; the UI reports when the cap is being hit.

## Asset lookup

Shaders and configs are read at runtime. They are deliberately **not** copied
beside the executable by a CMake `POST_BUILD` step, because CMake wraps those in
a nested `cmd /C "cd /D <path> && ..."` and this repository may live under a
directory containing an ampersand (`AI & Media`), which `cmd` re-parses as a
command separator. `engine::resolveAsset` instead searches, in order: the
current working directory, the executable's directory and its parent, then the
`GRAVITYSIM_SOURCE_DIR` baked in at configure time. A side benefit is that
shaders can be edited in the source tree and hot-reloaded without rebuilding.

## Physics vs visualisation

The warped grid is a **visual analogy only**. It is computed in a vertex shader
from body positions and masses and is a one-way read of simulation state. No
part of `sim/` reads the grid, and the grid's deformation function is not a
solution of the Einstein field equations. Orbital motion is Newtonian
throughout. See `docs/PHYSICS.md`.

## Future relativistic extension

The seam for a relativistic model is `sim::Integrator` plus a force/geodesic
provider interface. `GravitySystem::computeAccelerations` is the single place
where `a = -G m / r^2` appears, so a Schwarzschild geodesic integrator would
replace that one function rather than being threaded through the renderer.
