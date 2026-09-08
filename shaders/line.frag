#version 330 core

in float vFade;

uniform vec4 uColor;

out vec4 fragColor;

void main() {
    // Older trail samples fade out, so a trail reads as a direction of travel
    // rather than an undifferentiated loop.
    fragColor = vec4(uColor.rgb, uColor.a * vFade);
}
