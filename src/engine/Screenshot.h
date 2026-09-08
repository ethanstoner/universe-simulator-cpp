#pragma once

#include <string>

namespace engine {

// Reads the current default framebuffer back with glReadPixels and writes a
// PNG. Used both by the F12 key and by the --screenshot headless mode that the
// build scripts use to visually verify rendering milestones.
bool captureFramebufferToPng(int width, int height, const std::string& path);

// Minimal PNG writer: 8-bit RGB, zlib "stored" (uncompressed) deflate blocks.
// Avoids pulling in a compression library for what is a debug facility.
bool writePng(const std::string& path, int width, int height,
              const unsigned char* rgb);

}  // namespace engine
