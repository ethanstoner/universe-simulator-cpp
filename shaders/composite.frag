#version 330 core

// Final resolve: scene + bloom, exposure, tone map, vignette, gamma.

in vec2 vUv;

uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uBloomIntensity;
uniform float uExposure;
uniform float uVignette;
uniform int uTonemap;      // 0 = none, 1 = ACES approximation

out vec4 fragColor;

// Narkowicz's ACES filmic curve. Cheap, and it rolls highlights off instead of
// clipping them, which matters here because a star's core is deliberately far
// above 1.0 so that it blooms.
vec3 acesFilmic(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 scene = texture(uScene, vUv).rgb;
    vec3 bloom = texture(uBloom, vUv).rgb;

    vec3 color = scene + bloom * uBloomIntensity;
    color *= uExposure;

    if (uTonemap == 1) {
        color = acesFilmic(color);
    } else {
        color = clamp(color, 0.0, 1.0);
    }

    // Subtle darkening towards the corners. Keeps attention on the middle of
    // the scene without being obvious enough to look like a filter.
    if (uVignette > 0.0) {
        vec2 centred = vUv - 0.5;
        float falloff = 1.0 - uVignette * dot(centred, centred) * 2.0;
        color *= clamp(falloff, 0.0, 1.0);
    }

    // The HDR buffer is linear; the default framebuffer is sRGB-ish, so encode.
    color = pow(max(color, 0.0), vec3(1.0 / 2.2));
    fragColor = vec4(color, 1.0);
}
