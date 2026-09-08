# Physics: what this simulates, and what it does not

## The short version

The dynamics are **Newtonian**. Every body is accelerated by

```
a_i = sum over j != i of  G m_j (x_j - x_i) / |x_j - x_i|^3
```

integrated with a fixed timestep. There are no relativistic corrections
anywhere in the simulation.

The warped grid is a **visualisation**. It is a rubber-sheet analogy, not
general relativity, and nothing in the physics reads it.

## Units and constants

Strictly SI: metres, kilograms, seconds.

| Constant | Value | Source |
| --- | --- | --- |
| `G` | 6.674 30e-11 m^3 kg^-1 s^-2 | CODATA 2018 |
| `c` | 299 792 458 m/s | exact by definition |
| AU | 149 597 870 700 m | exact by IAU definition |
| Solar mass | 1.988 92e30 kg | |
| Earth mass | 5.972 19e24 kg | |
| Earth radius | 6 371 000 m | volumetric mean |

Positions, velocities and accelerations are `double`. This is not stylistic:
Earth's orbital radius is ~1.5e11 m, and `float` carries about seven
significant decimal digits, which would quantise Earth's position into steps of
roughly 16 000 m and make a stable orbit impossible to represent.

## Integration

Four integrators are available and can be switched at runtime:

| Method | Order | Symplectic | Behaviour on a long orbit |
| --- | --- | --- | --- |
| Explicit Euler | 1 | no | Energy grows without bound; orbits spiral outwards |
| Symplectic Euler | 1 | yes | Energy error bounded and oscillating |
| Velocity Verlet | 2 | yes | Bounded, time reversible -- **the default** |
| Runge-Kutta 4 | 4 | no | Very accurate short term, drifts secularly |

The default is velocity Verlet. RK4 has a smaller error per step but is not
symplectic, so over thousands of orbits its energy error accumulates in one
direction while Verlet's merely oscillates.

`tests/test_integrators.cpp` checks this rather than asserting it: explicit
Euler grows a harmonic oscillator's energy from 0.5 to over 10 in 10 000 steps
while symplectic Euler stays within 0.45-0.55, and the two-body energy test
compares the peak drift over orbits 1-25 against orbits 26-50 to confirm
boundedness rather than mere smallness.

### Timestep and time acceleration

The physics step is **fixed**. Time acceleration multiplies how much simulated
time a frame must cover, which becomes *more steps*, never a *larger step*:

```
accumulator += frameDelta * timeScale
while (accumulator >= fixedDt && steps < maxStepsPerFrame) {
    system.step(fixedDt);
    accumulator -= fixedDt;
}
```

Scaling `dt` with `timeScale` is the standard way to make an orbit that looks
stable at 1x fly apart at 1000x. There is a `maxStepsPerFrame` cap so an extreme
time scale degrades into slow motion instead of freezing the process, and the UI
reports when the cap is being hit rather than letting it look like correct
behaviour.

## Avoiding the 1/r^2 singularity

Three strategies, selectable at runtime. Whichever is chosen is a **documented
modification of Newtonian gravity**, not a silent fudge.

**None** -- raw `1/r^2`. A direct hit produces infinities. Kept because it is
the honest baseline, and `tests/test_scenes.cpp` deliberately verifies that it
*does* blow up: if that test ever stops failing, the other stabilisation tests
have stopped proving anything.

**Plummer softening** (default) -- the separation is softened:

```
a = G m r / (r^2 + eps^2)^(3/2)
```

The acceleration is now finite everywhere, peaking near `r ~ eps` instead of
diverging. The default `eps` is 1 000 km, far below every real separation in the
presets (the closest is the Earth-Moon distance at 3.84e8 m), so it changes
nothing observable while still bounding the force if a body is spawned on top of
a planet.

**Minimum distance** -- the separation used in the denominator is clamped to a
floor, while the direction still comes from the true displacement.

The reported potential energy uses the *same* softening the force uses.
Otherwise the diagnostic would not be measuring the energy the integrator is
actually conserving.

## What close encounters really do

Softening prevents infinities. It does **not** make a close encounter accurate.

A fixed timestep can only resolve an encounter whose characteristic time
`r_periapsis / v_periapsis` is much longer than the step. When it is not, the
integrator produces large, essentially arbitrary energy errors. This is a real
limitation and it is easy to mistake for the softening "hiding" something.

The two are distinguishable by refining the step: truncation error at an
unresolved encounter falls as the step falls, whereas a masked singularity does
not improve. `compact_mass_encounter_error_is_timestep_resolution_not_a_broken_force`
measures the drift at 600 s, 150 s and 37.5 s through a pass with a ~3.6e9 m
periapsis and requires it to fall monotonically and by more than 10x overall.

The practical consequence: if you fling a compact object straight through a
star, expect the energy diagnostic to jump. That is the timestep, not the force
law. Reduce the fixed step in the Simulation panel and the jump shrinks.

## Diagnostics

```
KE = sum  1/2 m_i |v_i|^2
PE = sum over pairs  -G m_i m_j / r_ij        (softened as above)
p  = sum  m_i v_i
L  = sum  x_i cross m_i v_i
```

Energy drift is reported as `|E - E0| / |E0|` against the value captured when
the scene loaded. For the inner solar system over ten simulated years at a
1800 s step the peak drift is below 1e-5, which is asserted by a test.

Total momentum is conserved to rounding because each unordered pair is visited
once and the equal-and-opposite accelerations are applied together. Over 20 000
steps the measured relative change is around 4e-14.

Spawning a body changes the system's real total energy, so the reference is
re-captured on spawn. Reporting that step change as "drift" would be
meaningless.

## Collisions

Optional, and crude. Documented limitations:

- Contact is instantaneous; there is no contact duration or deformation.
- Bodies are rigid spheres. No tidal disruption, fragmentation or Roche limit.
- A **merge** conserves mass and linear momentum, places the result at the
  centre of mass, and adds volumes to get the new radius. The kinetic energy of
  the relative motion is discarded -- physically it becomes heat, which is not
  tracked. Angular momentum about the merged body's own centre is not retained
  because rotation is not modelled at all.
- An **elastic** collision exchanges impulse along the contact normal with a
  restitution coefficient, and separates overlapping bodies so they do not
  re-collide every step.

Real planetary radii are minuscule next to orbital separations, so at a contact
scale of 1x collisions essentially never happen by accident. The UI exposes a
contact-radius multiplier to make them reachable in a demonstration; that
multiplier is a *display-driven convenience* and is not physical.

## The spacetime grid is a visualisation

This deserves to be unambiguous.

The grid is displaced vertically by a softened Newtonian potential summed over
nearby bodies, evaluated per vertex in `shaders/grid.vert`:

```
y(x, z) = -sum_i  k m_i / sqrt(d_i^2 + a^2)
```

then passed through a smooth saturation so an extreme mass gives a deep well
with a soft floor rather than a spike.

What this is:

- A legible depiction of *where the potential is deep*.
- The familiar rubber-sheet demonstration, which is genuinely useful for
  intuition about gravitational wells.

What this is **not**:

- Not a solution of the Einstein field equations.
- Not an embedding diagram of a spatial slice of Schwarzschild spacetime. (The
  Flamm paraboloid is a specific surface with `z = sqrt(8 M (r - 2M))`; this is
  not that, and even the Flamm paraboloid depicts only a 2D spatial slice, not
  "what spacetime looks like".)
- Not what spacetime physically looks like. Spacetime curvature is a property of
  a four-dimensional manifold and is not a sheet bending into a third dimension.
- Not an input to the simulation. The data flow is strictly one way: the shader
  reads body positions and masses. Nothing in `sim/` can even see the grid.

The vertical gain is normalised so that the heaviest body on the patch produces
a well-proportioned funnel in every preset. That normalisation is arbitrary and
chosen for readability. The *shape* of each well is the softened potential; the
overall depth is a presentation choice.

## Compact objects are not black holes

The presets include a "Compact massive object" and a "Neutron-star-like" spawn
preset. These are **Newtonian point masses with a small radius**. Since the
dynamics are still `F = G m1 m2 / r^2`, calling them black holes would be wrong.

The Schwarzschild radius is computed and displayed:

```
r_s = 2 G M / c^2
```

For the Sun that is 2.95 km against a 696 000 km radius; for the Earth, 8.9 mm.
It is shown because it is a useful reference number, and it can be drawn as a
marker sphere at true scale with no exaggeration -- which is why it is invisible
for ordinary bodies. That is the correct result, and the UI says so.

**Not modelled:** event horizons, photon spheres, innermost stable circular
orbits, gravitational lensing, gravitational redshift, time dilation, frame
dragging, relativistic precession, gravitational-wave emission and inspiral, or
any post-Newtonian correction whatsoever.

A body can be configured inside its own Schwarzschild radius. In reality that
means a black hole; here it remains a Newtonian point mass, and the spawn panel
warns about exactly that.

## Known simplifications

- Orbits are initialised as circles at the semi-major axis. Real planets have
  non-zero eccentricity and specific orbital phases; the presets prioritise
  deterministic, legible initial conditions over ephemeris accuracy.
- All presets are coplanar (the XZ plane). Real orbital inclinations are not
  reproduced, so the spacetime grid can lie in the orbital plane.
- Bodies are point masses for the purpose of gravity. No oblateness, no
  quadrupole moments, no tidal torques, no rotation, no axial tilt.
- No radiation pressure, solar wind, drag, or non-gravitational forces.
- Lighting is deliberately **not** inverse-square. Scene scales span four orders
  of magnitude and a physical falloff would leave the outer planets black.
  Lighting is a readability device, not a radiometric model.
- Body radii are exaggerated by a power law for display. Gravity never reads a
  drawn radius; see `docs/ARCHITECTURE.md`.

## Future relativistic work

Deliberately not attempted. The seam for it is `GravitySystem::accelerations`,
which is the single place `a = G m / r^2` appears, plus the `ForceModel`
interface the integrators take. A geodesic integrator would replace that one
function rather than being threaded through the renderer.

Plausible order: Schwarzschild geodesics for test particles, then perihelion
precession as a validation case (Mercury's 43 arcseconds per century is the
classic test), then photon trajectories and lensing, then an accretion-disk
visualisation. None of that should start before the Newtonian simulator is
stable, and none of it is present today.
