#version 330 core

in float vBrightness;
in vec3 vTint;

out vec4 fragColor;

void main() {
    // Round off the point sprite and give it a soft falloff, so stars read as
    // points of light rather than as the little squares GL_POINTS actually are.
    vec2 offset = gl_PointCoord * 2.0 - 1.0;
    float radiusSq = dot(offset, offset);
    if (radiusSq > 1.0) discard;

    float falloff = 1.0 - smoothstep(0.0, 1.0, radiusSq);
    // Squared so the core stays tight and the halo stays faint.
    fragColor = vec4(vTint * vBrightness * falloff * falloff, 1.0);
}
