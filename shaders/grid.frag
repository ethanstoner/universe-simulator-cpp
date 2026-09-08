#version 330 core

in float vDepth;
in vec3 vWorldPos;
in float vRimFade;

uniform vec3 uShallowColor;
uniform vec3 uDeepColor;
uniform float uOpacity;
uniform float uFadeDistance;  // world units; the grid dims beyond this

out vec4 fragColor;

void main() {
    // Deep parts of the well are tinted differently so the depth reads even
    // when the grid is viewed nearly edge on.
    vec3 color = mix(uShallowColor, uDeepColor, clamp(vDepth * 1.6, 0.0, 1.0));

    // Distance fade, using the camera-relative position directly (the eye is at
    // the origin in this space). uFadeDistance is supplied by the CPU as the
    // distance to the far edge of the patch, NOT a multiple of the grid extent:
    // when the camera sits farther from the scene than the patch is wide -- the
    // full solar-system view, at 110 units out over a 140 unit patch -- a fade
    // keyed to the extent alone erased the entire sheet.
    float distance = length(vWorldPos);
    float distanceFade = 1.0 - smoothstep(uFadeDistance * 0.72, uFadeDistance, distance);

    float alpha = uOpacity * vRimFade * distanceFade;
    // Lines in the well are brightened slightly, which is what gives the
    // familiar funnel its sense of depth.
    alpha *= 0.55 + 0.75 * vDepth;

    if (alpha <= 0.002) discard;
    fragColor = vec4(color, alpha);
}
