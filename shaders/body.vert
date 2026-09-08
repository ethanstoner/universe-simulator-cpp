#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uViewProjection;
uniform mat3 uNormalMatrix;

out vec3 vViewPosition;  // camera-relative world position, in world units
out vec3 vNormal;

void main() {
    vec4 world = uModel * vec4(aPosition, 1.0);
    vViewPosition = world.xyz;
    vNormal = normalize(uNormalMatrix * aNormal);
    gl_Position = uViewProjection * world;
}
