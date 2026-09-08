#version 330 core

// Spacetime curvature VISUALISATION.
//
// This is a rubber-sheet analogy, not general relativity. The vertical
// displacement below is a smooth, bounded, hand-chosen function of the
// Newtonian potential; it is not a solution of the Einstein field equations and
// it is not what a spatial slice of Schwarzschild spacetime looks like. Nothing
// in the simulation reads this displacement: it is a one-way read of body
// positions and masses. See docs/PHYSICS.md.

layout(location = 0) in vec3 aPosition;  // unit XZ patch, y = 0
layout(location = 1) in vec3 aNormal;

const int MAX_WELLS = 24;

uniform mat4 uViewProjection;
uniform vec3 uWellPositions[MAX_WELLS];  // camera-relative, world units
uniform float uWellStrength[MAX_WELLS];  // proportional to GM, in world units
uniform float uWellRadius[MAX_WELLS];    // softening radius, world units
uniform int uWellCount;

uniform vec3 uGridOrigin;   // camera-relative centre of the patch
uniform float uGridExtent;  // half-width, world units
uniform float uMaxDepth;    // displacement clamp, world units
uniform float uStrength;    // global multiplier

out float vDepth;      // 0..1, how far this vertex sank
out vec3 vWorldPos;
out float vRimFade;    // fades the sheet out at its edges

// Depth contribution of one well.
//
// The Newtonian potential -GM/r diverges at r = 0, so it is softened the same
// way the force is: -GM / sqrt(r^2 + a^2). That gives a smooth funnel with a
// finite floor of -GM/a, which is exactly the "bounded and non-singular"
// property the visualisation needs.
float wellDepth(vec3 gridPoint, int i) {
    vec3 delta = gridPoint - uWellPositions[i];
    // Distance measured in the grid plane: the sheet represents the orbital
    // plane, so a body lifted out of that plane should still dimple it beneath
    // itself rather than dragging the sheet up to meet it.
    float planar = length(delta.xz);
    float softened = sqrt(planar * planar + uWellRadius[i] * uWellRadius[i]);
    return -uWellStrength[i] / softened;
}

void main() {
    vec3 gridPoint = uGridOrigin + vec3(aPosition.x * uGridExtent, 0.0,
                                        aPosition.z * uGridExtent);

    float depth = 0.0;
    for (int i = 0; i < uWellCount && i < MAX_WELLS; ++i) {
        depth += wellDepth(gridPoint, i);
    }
    depth *= uStrength;

    // Clamp with a smooth saturation rather than a hard min(), so an extreme
    // mass produces a deep well with a soft floor instead of a flat disc with a
    // visible crease at the clamp boundary.
    float saturated = -uMaxDepth * (1.0 - exp(depth / max(uMaxDepth, 1e-6)));

    // Fade the patch out towards its rim so it does not end in a hard square.
    float edge = max(abs(aPosition.x), abs(aPosition.z));
    vRimFade = 1.0 - smoothstep(0.75, 1.0, edge);

    vDepth = clamp(-saturated / max(uMaxDepth, 1e-6), 0.0, 1.0);
    vec3 displaced = gridPoint + vec3(0.0, saturated, 0.0);
    vWorldPos = displaced;
    gl_Position = uViewProjection * vec4(displaced, 1.0);
}
