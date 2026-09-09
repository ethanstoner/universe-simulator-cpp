#pragma once

#include "render/Mesh.h"
#include "render/RenderSettings.h"
#include "render/Shader.h"

namespace render {

// HDR render target plus a bloom chain.
//
// The scene is drawn into a multisampled RGBA16F framebuffer rather than
// straight to the window. Two reasons it has to be floating point: a star's
// core is deliberately far brighter than 1.0 so that it blooms, and an 8-bit
// target would clip that away before the bright pass ever sees it.
//
// Order of operations:
//   scene -> MSAA HDR -> blit resolve -> bright pass -> ping-pong blur
//         -> composite (+ tone map, vignette, gamma) -> default framebuffer
//
// ImGui draws afterwards, straight to the default framebuffer, so the UI is
// never tone mapped or bloomed.
class PostProcess {
public:
    ~PostProcess();

    bool initialize();
    void reloadShaders();

    // Recreates the targets if the size or sample count changed. Safe to call
    // every frame.
    bool resize(int width, int height, int samples);

    // Binds the HDR target and clears it. Returns false if it is unusable, in
    // which case the caller should draw directly to the default framebuffer.
    bool begin(const glm::vec3& clearColor);

    // Resolves, blooms and composites to the default framebuffer.
    void end(const RenderSettings& settings);

    bool ready() const { return ready_; }
    const std::string& lastError() const { return lastError_; }

private:
    void release();
    bool createTargets(int width, int height, int samples);
    void drawFullscreen() const;

    Shader brightShader_;
    Shader blurShader_;
    Shader compositeShader_;

    // Attribute-less draws still need a bound VAO in a core profile.
    unsigned int emptyVao_ = 0;

    unsigned int msaaFbo_ = 0;
    unsigned int msaaColor_ = 0;
    unsigned int msaaDepth_ = 0;

    unsigned int resolveFbo_ = 0;
    unsigned int resolveColor_ = 0;

    // Bloom works at half resolution: the blur is a low-pass filter anyway, so
    // the detail thrown away is detail the effect would have discarded.
    unsigned int bloomFbo_[2] = {0, 0};
    unsigned int bloomColor_[2] = {0, 0};

    int width_ = 0;
    int height_ = 0;
    int bloomWidth_ = 0;
    int bloomHeight_ = 0;
    int samples_ = 0;
    bool ready_ = false;
    std::string lastError_;
};

}  // namespace render
