#version 330 core

// Attribute-less full-screen triangle. One oversized triangle rather than two
// quad triangles: it avoids the diagonal seam where the two halves meet, which
// costs a duplicated row of fragment work and can show up as a visible line
// after tone mapping. Core profile still requires a VAO to be bound, but it
// needs no vertex buffer.

out vec2 vUv;

void main() {
    vec2 uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vUv = uv;
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
