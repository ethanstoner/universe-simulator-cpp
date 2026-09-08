#version 330 core

in vec3 vNormal;

uniform vec4 uColor;

out vec4 fragColor;

void main() {
    fragColor = uColor;
}
