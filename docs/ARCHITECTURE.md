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

`sim/` is built as a separate static library (`simcore`) and the unit tests link
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
| `src/sim/ViewMath.*` | Camera-relative positioning, drawn-radius power law, ray picking. Lives in `sim/` only so it can be tested without a GL context |
| `src/engine/*` | Window, Input, Clock, TimeControl, Camera, AssetPaths, Screenshot, Application |
| `src/render/Shader,Mesh,MeshFactory` | GL resource wrappers and procedural geometry |
| `src/render/Renderer.*` | Draw orchestration and the camera-relative invariant |
| `src/render/GridRenderer.*` | The spacetime-analogy sheet |
| `src/render/TrailRenderer.*` | Fading orbit trails, optionally in a body's frame |
| `src/render/LineRenderer.*` | Batched arrows, rings and boxes |
| `src/render/PostProcess.*` | HDR target, bloom chain, tone map, vignette |
| `src/render/Starfield.*` | Decorative fixed-seed background stars |
| `src/ui/DebugUI.*` | The ImGui control panels and body labels |

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
| `metresPerUnit` | metres -> world units for the GPU | position transform only |
| `bodyVisualGain` / `bodyVisualExponent` | drawn-radius exaggeration | model matrix only |

**Gravity uses mass and position only.** The display scales live in
`render::RenderSettings`, which nothing in `sim/` includes.

The exaggeration is a *power law*, `gain * (r / metresPerUnit) ^ exponent`, not
a multiplier. A multiplier cannot work: the Sun is 109 Earth radii, so any
factor large enough to make the Earth visible draws the Sun wider than the
Earth's entire orbit. The exponent compresses that ratio while preserving the
ordering, and each scene solves its own gain from a named reference body --
which must be named explicitly, because the Earth-Moon preset still contains the
Sun and solving from "largest" made the Earth six pixels across.

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
`UNIVERSE_SIM_SOURCE_DIR` baked in at configure time. A side benefit is that
shaders can be edited in the source tree and hot-reloaded without rebuilding.

## Render pipeline

The scene is not drawn straight to the window. It goes into a multisampled
`RGBA16F` framebuffer first:

```
starfield -> grid -> bodies -> trails -> annotations     (into MSAA HDR target)
  -> blit resolve -> bright pass -> ping-pong Gaussian blur (half res)
  -> composite: scene + bloom, exposure, ACES tone map, vignette, gamma
  -> default framebuffer -> ImGui
```

Floating point is the point: a star's fragment shader deliberately outputs a
colour well above 1.0 so that the bright pass can find it. An 8-bit target would
clip that to white before bloom ever saw it, and the halo would vanish.

ImGui draws *after* the composite, straight to the default framebuffer, so the
panels are never tone mapped, bloomed or vignetted. If framebuffer creation
fails the renderer falls back to drawing directly to the window: bloom and tone
mapping are lost, the scene is not. Both paths are exercised
(`--no-post` takes the fallback).

The starfield is decorative. It is generated once from a fixed seed so the sky
is identical on every run, which keeps captured frames comparable; the stars are
not bodies and take no part in the simulation.

## Physics vs visualisation

The warped grid is a **visual analogy only**. It is computed in a vertex shader
from body positions and masses and is a one-way read of simulation state. No
part of `sim/` reads the grid, and the grid's deformation function is not a
solution of the Einstein field equations. Orbital motion is Newtonian
throughout. See `docs/PHYSICS.md`.

## Future relativistic extension

The seam for a relativistic model is the `sim::ForceModel` interface the
integrators take. `GravitySystem::accelerations` is the single place where
`a = G m / r^2` appears, so a Schwarzschild geodesic integrator would replace
that one function rather than being threaded through the renderer.
