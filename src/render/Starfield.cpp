#include "render/Starfield.h"

#include <glad/gl.h>

#include <cmath>
#include <cstdint>

namespace render {
namespace {

// A fixed-seed generator rather than std::random_device: the sky must be
// identical on every run so that captured frames can be compared.
struct Lcg {
    std::uint64_t state = 0x9E3779B97F4A7C15ull;

    double next() {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        // Top 53 bits give a uniform double in [0, 1).
        return static_cast<double>(state >> 11) / 9007199254740992.0;
    }
    double range(double low, double high) { return low + next() * (high - low); }
};

}  // namespace

void Starfield::build(int count) {
    Lcg random;
    std::vector<Vertex> vertices;
    vertices.reserve(static_cast<std::size_t>(count));

    for (int i = 0; i < count; ++i) {
        // Uniform on the sphere. Sampling latitude directly instead of its
        // cosine would bunch stars at the poles.
        const double z = random.range(-1.0, 1.0);
        const double phi = random.range(0.0, 6.283185307179586);
        const double r = std::sqrt(std::max(0.0, 1.0 - z * z));
        const glm::vec3 direction(static_cast<float>(r * std::cos(phi)),
                                  static_cast<float>(z),
                                  static_cast<float>(r * std::sin(phi)));

        // Brightness follows a steep power law, so there are a handful of
        // bright stars and a great many faint ones, as on a real sky. A few
        // exceed 1.0 and therefore pick up a little bloom.
        const double u = random.next();
        const float brightness = static_cast<float>(0.10 + 1.5 * std::pow(u, 6.0));
        const float size = static_cast<float>(1.0 + 2.2 * std::pow(u, 8.0));
        const float temperature = static_cast<float>(random.next());

        vertices.push_back({direction, glm::vec3(brightness, size, temperature)});
    }

    mesh_.upload(vertices);
    count_ = count;
}

bool Starfield::initialize(int count) {
    if (!shader_.loadFromFiles("shaders/star.vert", "shaders/star.frag")) return false;
    build(count);
    return true;
}

void Starfield::draw(const glm::mat4& viewProjection, const RenderSettings& settings) {
    if (!shader_.valid() || !settings.showStarfield || settings.starfieldBrightness <= 0.0f) {
        return;
    }

    shader_.bind();
    shader_.setMat4("uViewProjection", viewProjection);
    // Just inside the far plane: far enough that no scene geometry is behind
    // it, close enough not to be clipped away.
    shader_.setFloat("uDistance", 9000.0f);
    shader_.setFloat("uSizeScale", settings.starfieldSize);
    shader_.setFloat("uBrightness", settings.starfieldBrightness);

    glEnable(GL_PROGRAM_POINT_SIZE);
    // Additive, and no depth write: stars are a backdrop, not geometry.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    mesh_.draw(DrawMode::Points);

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_PROGRAM_POINT_SIZE);
}

}  // namespace render
