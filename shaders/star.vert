#version 330 core

// Background starfield. The positions are unit directions; the camera sits at
// the origin of the camera-relative space, so multiplying by the view-projection
// alone places them on the sky with no parallax, which is what "infinitely far"
// should look like.

layout(location = 0) in vec3 aDirection;
layout(location = 1) in vec3 aAttributes;  // x = brightness, y = size, z = colour temp

uniform mat4 uViewProjection;
uniform float uDistance;
uniform float uSizeScale;
uniform float uBrightness;

out float vBrightness;
out vec3 vTint;

void main() {
    vBrightness = aAttributes.x * uBrightness;

    // Crude blackbody-ish tint: 0 is cool and blue, 1 is warm and orange.
    float temperature = aAttributes.z;
    vTint = mix(vec3(0.72, 0.81, 1.00), vec3(1.00, 0.83, 0.66), temperature);

    gl_PointSize = max(aAttributes.y * uSizeScale, 1.0);
    gl_Position = uViewProjection * vec4(aDirection * uDistance, 1.0);
}
