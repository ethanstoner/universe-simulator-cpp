#version 330 core

// Trails and other line geometry are uploaded already camera-relative, in world
// units, with the per-vertex fade packed into the normal slot's x channel.

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aAttributes;  // x = fade 0..1

uniform mat4 uViewProjection;
uniform mat4 uModel;

out float vFade;

void main() {
    vFade = aAttributes.x;
    gl_Position = uViewProjection * uModel * vec4(aPosition, 1.0);
}
