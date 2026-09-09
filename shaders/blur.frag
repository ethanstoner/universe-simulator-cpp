#version 330 core

// One axis of a separable Gaussian blur. Run twice per iteration (horizontal
// then vertical), which is 2N taps instead of N*N for the 2D kernel.

in vec2 vUv;

uniform sampler2D uSource;
uniform vec2 uTexelSize;
uniform vec2 uDirection;   // (1,0) horizontal, (0,1) vertical

out vec4 fragColor;

// 9-tap Gaussian collapsed to 5 samples using linear-filtering tricks: each
// offset sample sits between two texels so the hardware's bilinear filter
// returns their weighted sum for free.
const float kOffsets[3] = float[](0.0, 1.3846153846, 3.2307692308);
const float kWeights[3] = float[](0.2270270270, 0.3162162162, 0.0702702703);

void main() {
    vec2 step = uTexelSize * uDirection;
    vec3 result = texture(uSource, vUv).rgb * kWeights[0];
    for (int i = 1; i < 3; ++i) {
        vec2 offset = step * kOffsets[i];
        result += texture(uSource, vUv + offset).rgb * kWeights[i];
        result += texture(uSource, vUv - offset).rgb * kWeights[i];
    }
    fragColor = vec4(result, 1.0);
}
