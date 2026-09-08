#include "engine/Screenshot.h"

#include <glad/gl.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace engine {
namespace {

unsigned long crc32Of(const unsigned char* data, size_t length, unsigned long crc = 0) {
    static unsigned long table[256];
    static bool built = false;
    if (!built) {
        for (unsigned long n = 0; n < 256; ++n) {
            unsigned long c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320UL ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        built = true;
    }
    crc = crc ^ 0xFFFFFFFFUL;
    for (size_t i = 0; i < length; ++i) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFUL;
}

void pushBigEndian32(std::vector<unsigned char>& out, unsigned long value) {
    out.push_back(static_cast<unsigned char>((value >> 24) & 0xFF));
    out.push_back(static_cast<unsigned char>((value >> 16) & 0xFF));
    out.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
    out.push_back(static_cast<unsigned char>(value & 0xFF));
}

void writeChunk(std::vector<unsigned char>& out, const char tag[4],
                const std::vector<unsigned char>& payload) {
    pushBigEndian32(out, static_cast<unsigned long>(payload.size()));
    std::vector<unsigned char> tagged;
    tagged.insert(tagged.end(), tag, tag + 4);
    tagged.insert(tagged.end(), payload.begin(), payload.end());
    out.insert(out.end(), tagged.begin(), tagged.end());
    pushBigEndian32(out, crc32Of(tagged.data(), tagged.size()));
}

}  // namespace

bool writePng(const std::string& path, int width, int height,
              const unsigned char* rgb) {
    if (width <= 0 || height <= 0 || !rgb) return false;

    // Raw scanlines, each prefixed with filter type 0 (None).
    std::vector<unsigned char> raw;
    raw.reserve(static_cast<size_t>(height) * (static_cast<size_t>(width) * 3 + 1));
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);
        const unsigned char* row = rgb + static_cast<size_t>(y) * width * 3;
        raw.insert(raw.end(), row, row + static_cast<size_t>(width) * 3);
    }

    // zlib stream with stored (uncompressed) deflate blocks.
    std::vector<unsigned char> z;
    z.push_back(0x78);  // CMF: deflate, 32K window
    z.push_back(0x01);  // FLG: no dict, fastest
    const size_t kBlock = 65535;
    for (size_t offset = 0; offset < raw.size(); offset += kBlock) {
        const size_t len = (raw.size() - offset < kBlock) ? raw.size() - offset : kBlock;
        const bool last = (offset + len >= raw.size());
        z.push_back(last ? 1 : 0);
        z.push_back(static_cast<unsigned char>(len & 0xFF));
        z.push_back(static_cast<unsigned char>((len >> 8) & 0xFF));
        z.push_back(static_cast<unsigned char>((~len) & 0xFF));
        z.push_back(static_cast<unsigned char>(((~len) >> 8) & 0xFF));
        z.insert(z.end(), raw.begin() + static_cast<long>(offset),
                 raw.begin() + static_cast<long>(offset + len));
    }
    unsigned long s1 = 1, s2 = 0;
    for (unsigned char byte : raw) {
        s1 = (s1 + byte) % 65521;
        s2 = (s2 + s1) % 65521;
    }
    pushBigEndian32(z, (s2 << 16) | s1);

    std::vector<unsigned char> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<unsigned char> ihdr;
    pushBigEndian32(ihdr, static_cast<unsigned long>(width));
    pushBigEndian32(ihdr, static_cast<unsigned long>(height));
    ihdr.push_back(8);  // bit depth
    ihdr.push_back(2);  // colour type: truecolour RGB
    ihdr.push_back(0);  // deflate
    ihdr.push_back(0);  // adaptive filtering
    ihdr.push_back(0);  // no interlace
    writeChunk(png, "IHDR", ihdr);
    writeChunk(png, "IDAT", z);
    writeChunk(png, "IEND", {});

    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) {
        std::fprintf(stderr, "[screenshot] cannot open %s\n", path.c_str());
        return false;
    }
    const size_t written = std::fwrite(png.data(), 1, png.size(), file);
    std::fclose(file);
    return written == png.size();
}

bool captureFramebufferToPng(int width, int height, const std::string& path) {
    if (width <= 0 || height <= 0) return false;
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // OpenGL hands back bottom-up rows; PNG wants top-down.
    std::vector<unsigned char> flipped(pixels.size());
    const size_t stride = static_cast<size_t>(width) * 3;
    for (int y = 0; y < height; ++y) {
        std::memcpy(flipped.data() + static_cast<size_t>(y) * stride,
                    pixels.data() + static_cast<size_t>(height - 1 - y) * stride, stride);
    }
    const bool ok = writePng(path, width, height, flipped.data());
    if (ok) std::printf("[screenshot] wrote %s (%dx%d)\n", path.c_str(), width, height);
    return ok;
}

}  // namespace engine
