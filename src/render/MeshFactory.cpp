#include "render/MeshFactory.h"

#include <algorithm>
#include <cmath>

namespace render {
namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;
}  // namespace

MeshData makeCircle(int segments) {
    segments = std::max(segments, 3);
    MeshData data;
    data.vertices.reserve(static_cast<std::size_t>(segments) + 1);
    data.indices.reserve(static_cast<std::size_t>(segments) * 3);

    // Vertex 0 is the centre of the fan.
    data.vertices.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}});
    for (int i = 0; i < segments; ++i) {
        const float theta = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
        data.vertices.push_back(
            {{std::cos(theta), std::sin(theta), 0.0f}, {0.0f, 0.0f, 1.0f}});
    }

    // Triangle (centre, rim[i], rim[i+1]); the last one wraps back to rim[0].
    for (int i = 0; i < segments; ++i) {
        const std::uint32_t current = static_cast<std::uint32_t>(i + 1);
        const std::uint32_t next = static_cast<std::uint32_t>((i + 1) % segments + 1);
        data.indices.push_back(0);
        data.indices.push_back(current);
        data.indices.push_back(next);
    }
    return data;
}

MeshData makeCircleOutline(int segments) {
    segments = std::max(segments, 3);
    MeshData data;
    data.vertices.reserve(static_cast<std::size_t>(segments));
    for (int i = 0; i < segments; ++i) {
        const float theta = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
        data.vertices.push_back(
            {{std::cos(theta), std::sin(theta), 0.0f}, {0.0f, 0.0f, 1.0f}});
    }
    for (int i = 0; i < segments; ++i) {
        data.indices.push_back(static_cast<std::uint32_t>(i));
        data.indices.push_back(static_cast<std::uint32_t>((i + 1) % segments));
    }
    return data;
}

MeshData makeUvSphere(int latitudeSegments, int longitudeSegments) {
    // Clamped to keep a stray UI value from asking for a hundred-million-vertex
    // sphere; 96x192 is already far beyond what a screen-space dot needs.
    latitudeSegments = std::clamp(latitudeSegments, 3, 96);
    longitudeSegments = std::clamp(longitudeSegments, 3, 192);

    MeshData data;
    data.vertices.reserve(static_cast<std::size_t>(latitudeSegments + 1) *
                          static_cast<std::size_t>(longitudeSegments + 1));

    for (int lat = 0; lat <= latitudeSegments; ++lat) {
        const float theta = kPi * static_cast<float>(lat) / static_cast<float>(latitudeSegments);
        const float sinTheta = std::sin(theta);
        const float cosTheta = std::cos(theta);
        for (int lon = 0; lon <= longitudeSegments; ++lon) {
            const float phi =
                kTwoPi * static_cast<float>(lon) / static_cast<float>(longitudeSegments);
            const glm::vec3 position{sinTheta * std::cos(phi), cosTheta,
                                     sinTheta * std::sin(phi)};
            // On a unit sphere the outward normal is the position itself.
            data.vertices.push_back({position, position});
        }
    }

    const int stride = longitudeSegments + 1;
    for (int lat = 0; lat < latitudeSegments; ++lat) {
        for (int lon = 0; lon < longitudeSegments; ++lon) {
            const std::uint32_t topLeft = static_cast<std::uint32_t>(lat * stride + lon);
            const std::uint32_t topRight = topLeft + 1;
            const std::uint32_t bottomLeft = static_cast<std::uint32_t>((lat + 1) * stride + lon);
            const std::uint32_t bottomRight = bottomLeft + 1;

            // At the poles one edge of the quad collapses to a point, so emit
            // only the triangle with area.
            if (lat != 0) {
                data.indices.push_back(topLeft);
                data.indices.push_back(bottomLeft);
                data.indices.push_back(topRight);
            }
            if (lat != latitudeSegments - 1) {
                data.indices.push_back(topRight);
                data.indices.push_back(bottomLeft);
                data.indices.push_back(bottomRight);
            }
        }
    }
    return data;
}

MeshData makeBoxOutline() {
    MeshData data;
    for (int i = 0; i < 8; ++i) {
        data.vertices.push_back({{(i & 1) ? 0.5f : -0.5f, (i & 2) ? 0.5f : -0.5f,
                                  (i & 4) ? 0.5f : -0.5f},
                                 {0.0f, 1.0f, 0.0f}});
    }
    const std::uint32_t edges[24] = {0, 1, 1, 3, 3, 2, 2, 0,   // -Z face
                                     4, 5, 5, 7, 7, 6, 6, 4,   // +Z face
                                     0, 4, 1, 5, 2, 6, 3, 7};  // connectors
    data.indices.assign(std::begin(edges), std::end(edges));
    return data;
}

MeshData makeGridLines(int divisions) {
    divisions = std::clamp(divisions, 2, 400);
    MeshData data;
    const int lineCount = divisions + 1;
    data.vertices.reserve(static_cast<std::size_t>(lineCount) * lineCount);

    // A full lattice of vertices, so the vertex shader can displace each one
    // independently and both line families share the same displaced points.
    for (int z = 0; z <= divisions; ++z) {
        for (int x = 0; x <= divisions; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(divisions) * 2.0f - 1.0f;
            const float v = static_cast<float>(z) / static_cast<float>(divisions) * 2.0f - 1.0f;
            data.vertices.push_back({{u, 0.0f, v}, {0.0f, 1.0f, 0.0f}});
        }
    }

    auto index = [&](int x, int z) {
        return static_cast<std::uint32_t>(z * lineCount + x);
    };
    for (int z = 0; z <= divisions; ++z) {
        for (int x = 0; x < divisions; ++x) {
            data.indices.push_back(index(x, z));
            data.indices.push_back(index(x + 1, z));
        }
    }
    for (int x = 0; x <= divisions; ++x) {
        for (int z = 0; z < divisions; ++z) {
            data.indices.push_back(index(x, z));
            data.indices.push_back(index(x, z + 1));
        }
    }
    return data;
}

MeshData makeGridSurface(int divisions) {
    divisions = std::clamp(divisions, 2, 400);
    MeshData data;
    const int lineCount = divisions + 1;
    for (int z = 0; z <= divisions; ++z) {
        for (int x = 0; x <= divisions; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(divisions) * 2.0f - 1.0f;
            const float v = static_cast<float>(z) / static_cast<float>(divisions) * 2.0f - 1.0f;
            data.vertices.push_back({{u, 0.0f, v}, {0.0f, 1.0f, 0.0f}});
        }
    }
    for (int z = 0; z < divisions; ++z) {
        for (int x = 0; x < divisions; ++x) {
            const std::uint32_t a = static_cast<std::uint32_t>(z * lineCount + x);
            const std::uint32_t b = a + 1;
            const std::uint32_t c = static_cast<std::uint32_t>((z + 1) * lineCount + x);
            const std::uint32_t d = c + 1;
            data.indices.push_back(a);
            data.indices.push_back(c);
            data.indices.push_back(b);
            data.indices.push_back(b);
            data.indices.push_back(c);
            data.indices.push_back(d);
        }
    }
    return data;
}

Mesh uploadMesh(const MeshData& data, bool dynamic) {
    Mesh mesh;
    mesh.upload(data.vertices, data.indices, dynamic);
    return mesh;
}

}  // namespace render
