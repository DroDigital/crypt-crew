#pragma once
// Offscreen targets the renderer needs that raylib does not expose directly:
// a multisampled HDR framebuffer (MSAA 4x) resolved into an HDR texture.
#include <raylib.h>

namespace eng {

struct HdrTarget {
    unsigned int fbo = 0;
    Texture2D color{};    // RGBA16F
    unsigned int depthRb = 0;
    int width = 0, height = 0;
};

struct MsaaTarget {
    unsigned int fbo = 0;
    unsigned int colorRb = 0;
    unsigned int depthRb = 0;
    int samples = 0;
    int width = 0, height = 0;
};

bool glTargetsInit();  // loads the few GL entry points we need (after InitWindow)
HdrTarget createHdrTarget(int w, int h, bool withDepth);
void destroyHdrTarget(HdrTarget& t);
MsaaTarget createMsaaTarget(int w, int h, int samples);
void destroyMsaaTarget(MsaaTarget& t);
// Resolve multisampled color into an HDR texture target.
void resolveMsaa(const MsaaTarget& src, const HdrTarget& dst);
int maxMsaaSamples();

}  // namespace eng
