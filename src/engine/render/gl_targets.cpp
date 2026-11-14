#include "engine/render/gl_targets.h"

#include <rlgl.h>

#include "engine/core/log.h"

namespace eng {
namespace {

using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLbitfield = unsigned int;

constexpr GLenum GL_FRAMEBUFFER = 0x8D40;
constexpr GLenum GL_READ_FRAMEBUFFER = 0x8CA8;
constexpr GLenum GL_DRAW_FRAMEBUFFER = 0x8CA9;
constexpr GLenum GL_RENDERBUFFER = 0x8D41;
constexpr GLenum GL_RGBA16F = 0x881A;
constexpr GLenum GL_DEPTH_COMPONENT24 = 0x81A6;
constexpr GLenum GL_COLOR_ATTACHMENT0 = 0x8CE0;
constexpr GLenum GL_DEPTH_ATTACHMENT = 0x8D00;
constexpr GLenum GL_COLOR_BUFFER_BIT = 0x4000;
constexpr GLenum GL_NEAREST = 0x2600;
constexpr GLenum GL_FRAMEBUFFER_COMPLETE = 0x8CD5;
constexpr GLenum GL_MAX_SAMPLES = 0x8D57;

#if defined(_WIN32)
#define CC_APIENTRY __stdcall
#else
#define CC_APIENTRY
#endif

using PFNGenFramebuffers = void(CC_APIENTRY*)(GLsizei, GLuint*);
using PFNDeleteFramebuffers = void(CC_APIENTRY*)(GLsizei, const GLuint*);
using PFNBindFramebuffer = void(CC_APIENTRY*)(GLenum, GLuint);
using PFNGenRenderbuffers = void(CC_APIENTRY*)(GLsizei, GLuint*);
using PFNDeleteRenderbuffers = void(CC_APIENTRY*)(GLsizei, const GLuint*);
using PFNBindRenderbuffer = void(CC_APIENTRY*)(GLenum, GLuint);
using PFNRenderbufferStorageMultisample = void(CC_APIENTRY*)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
using PFNRenderbufferStorage = void(CC_APIENTRY*)(GLenum, GLenum, GLsizei, GLsizei);
using PFNFramebufferRenderbuffer = void(CC_APIENTRY*)(GLenum, GLenum, GLenum, GLuint);
using PFNCheckFramebufferStatus = GLenum(CC_APIENTRY*)(GLenum);
using PFNBlitFramebuffer = void(CC_APIENTRY*)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
using PFNGetIntegerv = void(CC_APIENTRY*)(GLenum, GLint*);

struct Gl {
    PFNGenFramebuffers GenFramebuffers = nullptr;
    PFNDeleteFramebuffers DeleteFramebuffers = nullptr;
    PFNBindFramebuffer BindFramebuffer = nullptr;
    PFNGenRenderbuffers GenRenderbuffers = nullptr;
    PFNDeleteRenderbuffers DeleteRenderbuffers = nullptr;
    PFNBindRenderbuffer BindRenderbuffer = nullptr;
    PFNRenderbufferStorageMultisample RenderbufferStorageMultisample = nullptr;
    PFNRenderbufferStorage RenderbufferStorage = nullptr;
    PFNFramebufferRenderbuffer FramebufferRenderbuffer = nullptr;
    PFNCheckFramebufferStatus CheckFramebufferStatus = nullptr;
    PFNBlitFramebuffer BlitFramebuffer = nullptr;
    PFNGetIntegerv GetIntegerv = nullptr;
    bool ok = false;
} gl;

template <typename T>
bool load(T& fn, const char* name) {
    fn = reinterpret_cast<T>(rlGetProcAddress(name));
    return fn != nullptr;
}

}  // namespace

bool glTargetsInit() {
    bool ok = true;
    ok &= load(gl.GenFramebuffers, "glGenFramebuffers");
    ok &= load(gl.DeleteFramebuffers, "glDeleteFramebuffers");
    ok &= load(gl.BindFramebuffer, "glBindFramebuffer");
    ok &= load(gl.GenRenderbuffers, "glGenRenderbuffers");
    ok &= load(gl.DeleteRenderbuffers, "glDeleteRenderbuffers");
    ok &= load(gl.BindRenderbuffer, "glBindRenderbuffer");
    ok &= load(gl.RenderbufferStorageMultisample, "glRenderbufferStorageMultisample");
    ok &= load(gl.RenderbufferStorage, "glRenderbufferStorage");
    ok &= load(gl.FramebufferRenderbuffer, "glFramebufferRenderbuffer");
    ok &= load(gl.CheckFramebufferStatus, "glCheckFramebufferStatus");
    ok &= load(gl.BlitFramebuffer, "glBlitFramebuffer");
    ok &= load(gl.GetIntegerv, "glGetIntegerv");
    gl.ok = ok;
    if (!ok) log::warn("GL entry points for MSAA targets missing; MSAA disabled");
    return ok;
}

int maxMsaaSamples() {
    if (!gl.ok) return 0;
    GLint v = 0;
    gl.GetIntegerv(GL_MAX_SAMPLES, &v);
    return v;
}

HdrTarget createHdrTarget(int w, int h, bool withDepth) {
    HdrTarget t;
    t.width = w;
    t.height = h;
    t.fbo = rlLoadFramebuffer();
    rlEnableFramebuffer(t.fbo);
    t.color.id = rlLoadTexture(nullptr, w, h, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, 1);
    t.color.width = w;
    t.color.height = h;
    t.color.mipmaps = 1;
    t.color.format = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
    rlTextureParameters(t.color.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_LINEAR);
    rlTextureParameters(t.color.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
    rlTextureParameters(t.color.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP);
    rlTextureParameters(t.color.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
    rlFramebufferAttach(t.fbo, t.color.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    if (withDepth) {
        t.depthRb = rlLoadTextureDepth(w, h, true);
        rlFramebufferAttach(t.fbo, t.depthRb, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
    }
    if (!rlFramebufferComplete(t.fbo)) log::error("HDR framebuffer incomplete (%dx%d)", w, h);
    rlDisableFramebuffer();
    return t;
}

void destroyHdrTarget(HdrTarget& t) {
    if (t.fbo) rlUnloadFramebuffer(t.fbo);  // also unloads attached textures/renderbuffers
    t = HdrTarget{};
}

MsaaTarget createMsaaTarget(int w, int h, int samples) {
    MsaaTarget t;
    if (!gl.ok || samples < 2) return t;
    t.width = w;
    t.height = h;
    t.samples = samples;
    gl.GenFramebuffers(1, &t.fbo);
    gl.BindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    gl.GenRenderbuffers(1, &t.colorRb);
    gl.BindRenderbuffer(GL_RENDERBUFFER, t.colorRb);
    gl.RenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA16F, w, h);
    gl.FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, t.colorRb);
    gl.GenRenderbuffers(1, &t.depthRb);
    gl.BindRenderbuffer(GL_RENDERBUFFER, t.depthRb);
    gl.RenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, w, h);
    gl.FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, t.depthRb);
    const bool complete = gl.CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    gl.BindRenderbuffer(GL_RENDERBUFFER, 0);
    gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!complete) {
        log::warn("MSAA framebuffer incomplete; falling back to no MSAA");
        destroyMsaaTarget(t);
    }
    return t;
}

void destroyMsaaTarget(MsaaTarget& t) {
    if (gl.ok) {
        if (t.colorRb) gl.DeleteRenderbuffers(1, &t.colorRb);
        if (t.depthRb) gl.DeleteRenderbuffers(1, &t.depthRb);
        if (t.fbo) gl.DeleteFramebuffers(1, &t.fbo);
    }
    t = MsaaTarget{};
}

void resolveMsaa(const MsaaTarget& src, const HdrTarget& dst) {
    if (!gl.ok || !src.fbo) return;
    gl.BindFramebuffer(GL_READ_FRAMEBUFFER, src.fbo);
    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, dst.fbo);
    gl.BlitFramebuffer(0, 0, src.width, src.height, 0, 0, dst.width, dst.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
}

}  // namespace eng
