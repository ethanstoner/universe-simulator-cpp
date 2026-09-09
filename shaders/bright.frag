#version 330 core

// Bright-pass: isolates what bloom should be built from.

in vec2 vUv;

uniform sampler2D uScene;
uniform float uThreshold;
uniform float uSoftKnee;   // width of the smooth roll-in below the threshold

out vec4 fragColor;

void main() {
    vec3 color = texture(uScene, vUv).rgb;

    // Perceptual luminance, so a saturated blue star is not treated as being
    // as bright as a white one of the same numeric magnitude.
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));

    // A hard threshold makes bloom pop in and out as a body's brightness
    // crosses it, which reads as flickering while the camera moves. The soft
    // knee ramps the contribution in over a band instead.
    float knee = max(uSoftKnee, 1e-4);
    float contribution = smoothstep(uThreshold - knee, uThreshold + knee, luminance);

    fragColor = vec4(color * contribution, 1.0);
}
