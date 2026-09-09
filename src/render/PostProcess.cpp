#include "render/PostProcess.h"

#include <glad/gl.h>

#include <algorithm>
#include <cstdio>

namespace render {
namespace {

// Creates a single-sample RGBA16F colour target with clamped, linearly filtered
// sampling. Clamping matters for the blur: a wrapped edge would smear the top
// of the frame into the bottom.
void makeColorTexture(unsigned int& texture, int width, int height) {
    if (!texture) glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

}  // namespace

PostProcess::~PostProcess() { release(); }

void PostProcess::release() {
    if (msaaFbo_) glDeleteFramebuffers(1, &msaaFbo_);
    if (resolveFbo_) glDeleteFramebuffers(1, &resolveFbo_);
    if (bloomFbo_[0]) glDeleteFramebuffers(2, bloomFbo_);
    if (msaaColor_) glDeleteTextures(1, &msaaColor_);
    if (resolveColor_) glDeleteTextures(1, &resolveColor_);
    if (bloomColor_[0]) glDeleteTextures(2, bloomColor_);
    if (msaaDepth_) glDeleteRenderbuffers(1, &msaaDepth_);
    msaaFbo_ = resolveFbo_ = 0;
    bloomFbo_[0] = bloomFbo_[1] = 0;
    msaaColor_ = resolveColor_ = msaaDepth_ = 0;
    bloomColor_[0] = bloomColor_[1] = 0;
    width_ = height_ = samples_ = 0;
}

bool PostProcess::initialize() {
    bool ok = brightShader_.loadFromFiles("shaders/fullscreen.vert", "shaders/bright.frag");
    ok = blurShader_.loadFromFiles("shaders/fullscreen.vert", "shaders/blur.frag") && ok;
    ok = compositeShader_.loadFromFiles("shaders/fullscreen.vert", "shaders/composite.frag") && ok;
    if (!emptyVao_) glGenVertexArrays(1, &emptyVao_);
    if (!ok) lastError_ = "post-process shaders failed to compile";
    return ok;
}

void PostProcess::reloadShaders() {
    brightShader_.reload();
    blurShader_.reload();
    compositeShader_.reload();
}

bool PostProcess::createTargets(int width, int height, int samples) {
    release();

    width_ = width;
    height_ = height;
    samples_ = samples;
    bloomWidth_ = std::max(width / 2, 1);
    bloomHeight_ = std::max(height / 2, 1);

    // --- multisampled HDR target the scene is drawn into ---------------------
    glGenFramebuffers(1, &msaaFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, msaaFbo_);

    glGenTextures(1, &msaaColor_);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msaaColor_);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_RGBA16F, width, height,
                            GL_TRUE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D_MULTISAMPLE, msaaColor_, 0);

    glGenRenderbuffers(1, &msaaDepth_);
    glBindRenderbuffer(GL_RENDERBUFFER, msaaDepth_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, width,
                                     height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              msaaDepth_);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        lastError_ = "multisampled HDR framebuffer incomplete";
        std::fprintf(stderr, "[post] %s (samples = %d)\n", lastError_.c_str(), samples);
        release();
        return false;
    }

    // --- single-sample resolve target ---------------------------------------
    glGenFramebuffers(1, &resolveFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, resolveFbo_);
    makeColorTexture(resolveColor_, width, height);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           resolveColor_, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        lastError_ = "resolve framebuffer incomplete";
        release();
        return false;
    }

    // --- half-resolution bloom ping-pong pair --------------------------------
    glGenFramebuffers(2, bloomFbo_);
    for (int i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo_[i]);
        makeColorTexture(bloomColor_[i], bloomWidth_, bloomHeight_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               bloomColor_[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            lastError_ = "bloom framebuffer incomplete";
            release();
            return false;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    lastError_.clear();
    return true;
}

bool PostProcess::resize(int width, int height, int samples) {
    if (width <= 0 || height <= 0) return false;
    samples = std::clamp(samples, 1, 8);
    if (width == width_ && height == height_ && samples == samples_ && msaaFbo_ != 0) {
        ready_ = true;
        return true;
    }
    ready_ = createTargets(width, height, samples);
    return ready_;
}

bool PostProcess::begin(const glm::vec3& clearColor) {
    if (!ready_ || !brightShader_.valid()) return false;
    glBindFramebuffer(GL_FRAMEBUFFER, msaaFbo_);
    glViewport(0, 0, width_, height_);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    return true;
}

void PostProcess::drawFullscreen() const {
    glBindVertexArray(emptyVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void PostProcess::end(const RenderSettings& settings) {
    if (!ready_) return;

    // Resolve the multisampled colour into a sampleable texture.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFbo_);
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);

    // Post-processing is pure 2D: depth and blending would both corrupt it.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    const bool wantBloom = settings.bloomEnabled && settings.bloomIntensity > 0.0f;

    if (wantBloom) {
        // Bright pass into the first bloom target.
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo_[0]);
        glViewport(0, 0, bloomWidth_, bloomHeight_);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        brightShader_.bind();
        brightShader_.setInt("uScene", 0);
        brightShader_.setFloat("uThreshold", settings.bloomThreshold);
        brightShader_.setFloat("uSoftKnee", settings.bloomSoftKnee);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, resolveColor_);
        drawFullscreen();

        // Separable blur, ping-ponging between the two half-res targets. Each
        // iteration is one horizontal and one vertical pass.
        blurShader_.bind();
        blurShader_.setInt("uSource", 0);
        blurShader_.setVec2("uTexelSize", glm::vec2(1.0f / static_cast<float>(bloomWidth_),
                                                    1.0f / static_cast<float>(bloomHeight_)));
        int source = 0;
        const int iterations = std::clamp(settings.bloomIterations, 1, 12);
        for (int i = 0; i < iterations * 2; ++i) {
            const int destination = 1 - source;
            glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo_[destination]);
            glClear(GL_COLOR_BUFFER_BIT);
            blurShader_.setVec2("uDirection", (i % 2 == 0) ? glm::vec2(1.0f, 0.0f)
                                                           : glm::vec2(0.0f, 1.0f));
            glBindTexture(GL_TEXTURE_2D, bloomColor_[source]);
            drawFullscreen();
            source = destination;
        }

        // Composite reads whichever target the loop finished writing.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width_, height_);
        compositeShader_.bind();
        compositeShader_.setInt("uBloom", 1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, bloomColor_[source]);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width_, height_);
        compositeShader_.bind();
        compositeShader_.setInt("uBloom", 1);
        // Sampling the resolve texture with zero intensity keeps the shader
        // free of a branch on an unbound sampler, which some drivers dislike.
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, resolveColor_);
    }

    compositeShader_.setInt("uScene", 0);
    compositeShader_.setFloat("uBloomIntensity", wantBloom ? settings.bloomIntensity : 0.0f);
    compositeShader_.setFloat("uExposure", settings.exposure);
    compositeShader_.setFloat("uVignette", settings.vignette);
    compositeShader_.setInt("uTonemap", settings.tonemap ? 1 : 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, resolveColor_);
    drawFullscreen();

    // Leave the pipeline as the rest of the frame expects to find it.
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
}

}  // namespace render
