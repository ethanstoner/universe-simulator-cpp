#pragma once

#include <cstdint>
#include <vector>

#include "render/Mesh.h"

namespace render {

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

// A filled unit circle in the XY plane, built as a triangle fan around a centre
// vertex: vertex 0 is the centre, vertices 1..segments lie on the rim at
// (cos t, sin t), and each triangle is (centre, rim[i], rim[i+1]).
MeshData makeCircle(int segments);

// A unit-radius UV sphere.
//   x = sin(theta) cos(phi),  y = cos(theta),  z = sin(theta) sin(phi)
// with theta in [0, pi] (latitude) and phi in [0, 2pi] (longitude). Each
// latitude/longitude cell becomes a quad drawn as two triangles. Poles are
// degenerate quads and their zero-area triangles are skipped.
MeshData makeUvSphere(int latitudeSegments, int longitudeSegments);

// A wireframe circle (line loop as a LINES list), used for orbit rings and the
// Schwarzschild radius marker.
MeshData makeCircleOutline(int segments);

// An axis-aligned wireframe box spanning the unit cube centred on the origin.
MeshData makeBoxOutline();

// A flat XZ grid of lines spanning [-1, 1] in both axes, with `divisions` cells
// per side. Emitted as a LINES list. The vertex shader displaces Y.
MeshData makeGridLines(int divisions);

// A subdivided flat XZ patch spanning [-1, 1], as triangles. Used when the
// spacetime grid is drawn as a shaded sheet rather than wireframe.
MeshData makeGridSurface(int divisions);

Mesh uploadMesh(const MeshData& data, bool dynamic = false);

}  // namespace render
