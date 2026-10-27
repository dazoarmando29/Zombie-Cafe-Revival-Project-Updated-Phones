/* OpenGL ES bridge: forward to real GLES1 (the game's context type).
 *
 * The engine's fixed-function calls go straight to libGLESv1_CM. Its shader
 * calls fail against the GLES1 context exactly like on the original devices
 * (the "shader compile error" from the startup log) and the engine falls back
 * to fixed-function rendering. Pointers live in emulated memory; the heap and
 * stack are zero-copy host aliases so most client-array data resolves
 * directly, anything else is copied into scratch buffers at draw time. */
#include "imports_gl.h"

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include <GLES/gl.h>
#include <android/log.h>
#include <dlfcn.h>
#include <unicorn/unicorn.h>

#include "emu_mem.h"
#include "stub_dispatch.h"
#include "diag.h"

#define TAG "zcr-gl"

/* OES entry points not in <GLES/gl.h> */
extern "C" {
void glCompressedTexImage2D(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint,
                            GLsizei, const GLvoid *);
void glActiveTexture(GLenum);
void glClientActiveTexture(GLenum);
void glBindFramebufferOES(GLenum, GLuint);
}

namespace {

uint32_t f2u(float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}
float u2f(uint32_t u) {
    float f;
    memcpy(&f, &u, 4);
    return f;
}

/* Resolve an emulated pointer for direct GL consumption (zero-copy when the
 * address lives in an aliased region); fall back to copying into buf. */
const GLvoid *gl_ptr(uc_engine *uc, uint32_t emu, size_t bytes,
                     std::vector<uint8_t> *buf) {
    if (void *p = emu::host_ptr(uc, emu)) return p;
    buf->resize(bytes);
    if (emu::read(uc, emu, buf->data(), bytes)) return buf->data();
    return nullptr;
}

/* ---- client arrays (GLES1 vertex pipeline) ---- */
struct ClientArray {
    bool enabled = false;
    GLint size = 4;
    GLenum type = GL_FLOAT;
    GLsizei stride = 0;
    uint32_t ptr = 0;
};
ClientArray g_va, g_tc, g_co, g_no;
int g_tex_env_mode = 0x2100; /* GL_MODULATE */

std::vector<uint8_t> g_tmp_va, g_tmp_tc, g_tmp_co, g_tmp_no, g_tmp_idx, g_tmp_misc;

void apply_arrays(uc_engine *uc, uint32_t first, uint32_t elems) {
    /* zero-filled fallback so a bad pointer never feeds garbage to the GPU */
    static std::vector<uint8_t> s_zero;
    if (s_zero.size() < 4096) s_zero.resize(4096, 0);

    auto resolve = [&](ClientArray &a, std::vector<uint8_t> &scratch,
                       const void **out) -> void {
        size_t esz = (a.type == GL_BYTE || a.type == GL_UNSIGNED_BYTE) ? 1
                   : (a.type == GL_SHORT || a.type == GL_UNSIGNED_SHORT) ? 2 : 4;
        size_t stride = a.stride ? a.stride : esz * a.size;
        size_t off = (size_t)first * stride;
        size_t bytes = (size_t)elems * stride + 32;
        if (void *hp = emu::host_ptr(uc, a.ptr)) {
            *out = (const uint8_t *)hp + off;
        } else {
            scratch.resize(off + bytes);
            if (!emu::read(uc, a.ptr, scratch.data(), scratch.size())) {
                *out = s_zero.data();
                return;
            }
            *out = scratch.data() + off;
        }
    };
    if (g_va.enabled && g_va.ptr) {
        const void *p = nullptr;
        resolve(g_va, g_tmp_va, &p);
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(g_va.size, g_va.type, g_va.stride, p);
    }
    if (g_tc.enabled && g_tc.ptr) {
        const void *p = nullptr;
        resolve(g_tc, g_tmp_tc, &p);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(g_tc.size, g_tc.type, g_tc.stride, p);
    }
    if (g_co.enabled && g_co.ptr) {
        const void *p = nullptr;
        resolve(g_co, g_tmp_co, &p);
        glEnableClientState(GL_COLOR_ARRAY);
        glColorPointer(g_co.size, g_co.type, g_co.stride, p);
    }
    if (g_no.enabled && g_no.ptr) {
        const void *p = nullptr;
        resolve(g_no, g_tmp_no, &p);
        glEnableClientState(GL_NORMAL_ARRAY);
        glNormalPointer(g_no.type, g_no.stride, p);
    }
}

}  // namespace

/* ================= matrix / state (direct GLES1) ================= */

static void h_glMatrixMode(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc;
    glMatrixMode((GLenum)a.r[0]);
    *rv = 0;
}
static void h_glPushMatrix(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc; (void)a;
    glPushMatrix();
    *rv = 0;
}
static void h_glPopMatrix(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc; (void)a;
    glPopMatrix();
    *rv = 0;
}
static void h_glLoadIdentity(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc; (void)a;
    glLoadIdentity();
    *rv = 0;
}
static void h_glLoadMatrixf(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::vector<float> m(16);
    emu::read(uc, a.r[0], m.data(), 64);
    glLoadMatrixf(m.data());
    *rv = 0;
}
static void h_glMultMatrixf(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::vector<float> m(16);
    emu::read(uc, a.r[0], m.data(), 64);
    glMultMatrixf(m.data());
    *rv = 0;
}
static void h_glTranslatef(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc;
    glTranslatef(u2f(a.r[0]), u2f(a.r[1]), u2f(a.r[2]));
    *rv = 0;
}
static void h_glScalef(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc;
    glScalef(u2f(a.r[0]), u2f(a.r[1]), u2f(a.r[2]));
    *rv = 0;
}
static void h_glRotatef(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc;
    glRotatef(u2f(a.r[0]), u2f(a.r[1]), u2f(a.r[2]), u2f(a.r[3]));
    *rv = 0;
}
static void h_glOrthof(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc;
    glOrthof(u2f(a.r[0]), u2f(a.r[1]), u2f(a.r[2]), u2f(a.arg(3)),
             u2f(a.arg(4)), u2f(a.arg(5)));
    *rv = 0;
}

static void h_glEnableClientState(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc;
    GLenum state = (GLenum)a.r[0];
    switch (state) {
        case GL_VERTEX_ARRAY: g_va.enabled = true; break;
        case GL_TEXTURE_COORD_ARRAY: g_tc.enabled = true; break;
        case GL_COLOR_ARRAY: g_co.enabled = true; break;
        case GL_NORMAL_ARRAY: g_no.enabled = true; break;
    }
    /* MUST reach the real context: with the array disabled GLES1 draws every
       vertex from one constant - degenerate triangles, zero fragments. */
    glEnableClientState(state);
    *rv = 0;
}
static void h_glDisableClientState(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc;
    GLenum state = (GLenum)a.r[0];
    switch (state) {
        case GL_VERTEX_ARRAY: g_va.enabled = false; break;
        case GL_TEXTURE_COORD_ARRAY: g_tc.enabled = false; break;
        case GL_COLOR_ARRAY: g_co.enabled = false; break;
        case GL_NORMAL_ARRAY: g_no.enabled = false; break;
    }
    glDisableClientState(state);
    *rv = 0;
}
static void h_glVertexPointer(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc;
    g_va.size = (GLint)a.r[0];
    g_va.type = (GLenum)a.r[1];
    g_va.stride = (GLsizei)a.r[2];
    g_va.ptr = a.r[3];
    *rv = 0;
}
static void h_glTexCoordPointer(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc;
    g_tc.size = (GLint)a.r[0];
    g_tc.type = (GLenum)a.r[1];
    g_tc.stride = (GLsizei)a.r[2];
    g_tc.ptr = a.r[3];
    *rv = 0;
}
static void h_glColorPointer(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc;
    g_co.size = (GLint)a.r[0];
    g_co.type = (GLenum)a.r[1];
    g_co.stride = (GLsizei)a.r[2];
    g_co.ptr = a.r[3];
    *rv = 0;
}
static void h_glNormalPointer(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc;
    g_no.type = (GLenum)a.r[0];
    g_no.stride = (GLsizei)a.r[1];
    g_no.ptr = a.r[2];
    g_no.size = 3;
    *rv = 0;
}

static void h_glColor4f(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc;
    static int logged = 0;
    if (logged++ < 10)
        diag_log("[color] 4f=(%f,%f,%f,%f)", u2f(a.r[0]), u2f(a.r[1]),
                 u2f(a.r[2]), u2f(a.r[3]));
    glColor4f(u2f(a.r[0]), u2f(a.r[1]), u2f(a.r[2]), u2f(a.r[3]));
    *rv = 0;
}
static void h_glColor4ub(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc;
    static int logged = 0;
    if (logged++ < 10)
        diag_log("[color] 4ub=(%u,%u,%u,%u)", a.r[0] & 0xFF, a.r[1] & 0xFF,
                 a.r[2] & 0xFF, a.r[3] & 0xFF);
    glColor4ub((GLubyte)a.r[0], (GLubyte)a.r[1], (GLubyte)a.r[2], (GLubyte)a.r[3]);
    *rv = 0;
}
static void h_glTexEnvi(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc;
    if (a.r[1] == 0x2200) g_tex_env_mode = (int)a.r[2];
    glTexEnvi((GLenum)a.r[0], (GLenum)a.r[1], (GLint)a.r[2]);
    *rv = 0;
}
static void h_glMaterialf(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc;
    glMaterialf((GLenum)a.r[0], (GLenum)a.r[1], u2f(a.r[2]));
    *rv = 0;
}
static void h_glMaterialfv(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::vector<float> v(4);
    emu::read(uc, a.r[2], v.data(), 16);
    glMaterialfv((GLenum)a.r[0], (GLenum)a.r[1], v.data());
    *rv = 0;
}
static void h_glLightfv(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::vector<float> v(4);
    emu::read(uc, a.r[2], v.data(), 16);
    glLightfv((GLenum)a.r[0], (GLenum)a.r[1], v.data());
    *rv = 0;
}

static void h_glDrawArrays(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    diag_note_draw();
    apply_arrays(uc, (uint32_t)a.r[1], (uint32_t)a.r[2]);
    /* diag: sample the resolved vertex + color data of the first draw */
    static int logged = 0;
    if (logged < 3 && g_va.enabled && g_va.ptr) {
        float v[4] = {0};
        emu::read(uc, g_va.ptr, v, 16);
        unsigned char c[4] = {0};
        if (g_co.enabled && g_co.ptr) emu::read(uc, g_co.ptr, c, 4);
        diag_log("[draw] mode=%u first=%d count=%d vptr=0x%x vert=(%f,%f,%f) col=(%u,%u,%u,%u) blend=%d",
                 a.r[0], (int)a.r[1], (int)a.r[2], g_va.ptr, v[0], v[1], v[2],
                 c[0], c[1], c[2], c[3], glIsEnabled(GL_BLEND));
        logged++;
    }
    glDrawArrays((GLenum)a.r[0], (GLint)a.r[1], (GLsizei)a.r[2]);
    *rv = 0;
}
static void h_glDrawElements(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    diag_note_draw();
    GLenum mode = (GLenum)a.r[0];
    GLsizei count = (GLsizei)a.r[1];
    GLenum type = (GLenum)a.r[2];
    uint32_t idx_ptr = a.r[3];
    if (!count || !idx_ptr) {
        *rv = 0;
        return;
    }
    size_t isz = (type == GL_UNSIGNED_SHORT) ? 2 : 1;
    /* find the max referenced index so array copies cover it */
    std::vector<uint8_t> idx((size_t)count * isz);
    if (!emu::read(uc, idx_ptr, idx.data(), idx.size())) {
        diag_log("[draw] ELEM index read FAILED ptr=0x%x count=%d type=%u",
                 idx_ptr, (int)count, type);
        *rv = 0;
        return;
    }
    uint32_t max_idx = 0;
    for (GLsizei i = 0; i < count; i++) {
        uint32_t v = (isz == 2) ? (idx[i * 2] | (idx[i * 2 + 1] << 8)) : idx[i];
        if (v > max_idx) max_idx = v;
    }
    static int logged = 0;
    if (logged < 4) {
        float v[8] = {0};
        emu::read(uc, g_va.ptr, v, 32);
        unsigned char c[8] = {0};
        if (g_co.enabled && g_co.ptr) emu::read(uc, g_co.ptr, c, 8);
        float t[4] = {0};
        if (g_tc.enabled && g_tc.ptr) emu::read(uc, g_tc.ptr, t, 16);
        float proj[16], mv[16];
        glGetFloatv(GL_PROJECTION_MATRIX, proj);
        glGetFloatv(GL_MODELVIEW_MATRIX, mv);
        float cc[4] = {0};
        glGetFloatv(GL_CURRENT_COLOR, cc);
        diag_log("[draw] ELEM mode=%u count=%d va(%u,%u,%u) co_en=%d tc_en=%d vert=(%f,%f) col=(%u,%u,%u,%u) tcs=(%f,%f)",
                 mode, (int)count, g_va.size, g_va.type, g_va.stride,
                 g_co.enabled, g_tc.enabled, v[0], v[1], c[0], c[1], c[2], c[3],
                 t[0], t[1]);
        diag_log("[draw] proj=[%f %f %f %f | %f %f %f %f | %f %f %f %f | %f %f %f %f]",
                 proj[0], proj[4], proj[8], proj[12], proj[1], proj[5], proj[9],
                 proj[13], proj[2], proj[6], proj[10], proj[14], proj[3], proj[7],
                 proj[11], proj[15]);
        diag_log("[draw] mv=[%f %f %f %f | %f %f %f %f | %f %f %f %f | %f %f %f %f] curcol=(%f,%f,%f,%f) texenv=%d",
                 mv[0], mv[4], mv[8], mv[12], mv[1], mv[5], mv[9], mv[13],
                 mv[2], mv[6], mv[10], mv[14], mv[3], mv[7], mv[11], mv[15],
                 cc[0], cc[1], cc[2], cc[3], g_tex_env_mode);
        logged++;
    }
    apply_arrays(uc, 0, max_idx + 1);
    glDrawElements(mode, count, type, idx.data());
    *rv = 0;
}

/* ================= generic passthrough ================= */

#define P1(name, expr) \
    static void name(uc_engine *uc, const emu::Args &a, uint32_t *rv) { \
        (void)uc; expr; *rv = 0; }

P1(h_glEnable, glEnable((GLenum)a.r[0]))
P1(h_glDisable, glDisable((GLenum)a.r[0]))
P1(h_glClear, glClear((GLbitfield)a.r[0]))
P1(h_glBlendFunc, glBlendFunc((GLenum)a.r[0], (GLenum)a.r[1]))
P1(h_glScissor, glScissor((GLint)a.r[0], (GLint)a.r[1], (GLsizei)a.r[2], (GLsizei)a.r[3]))
P1(h_glLineWidth, glLineWidth(u2f(a.r[0])))
P1(h_glFinish, glFinish())
P1(h_glBindTexture, glBindTexture((GLenum)a.r[0], (GLuint)a.r[1]))
P1(h_glTexParameteri, glTexParameteri((GLenum)a.r[0], (GLenum)a.r[1], (GLint)a.r[2]))
P1(h_glActiveTexture, glActiveTexture((GLenum)a.r[0]))
P1(h_glClientActiveTexture, glClientActiveTexture((GLenum)a.r[0]))
P1(h_glBindFramebufferOES, glBindFramebufferOES((GLenum)a.r[0], (GLuint)a.r[1]))

static void h_glClearColor(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc;
    glClearColor(u2f(a.r[0]), u2f(a.r[1]), u2f(a.r[2]), u2f(a.r[3]));
    *rv = 0;
}
static void h_glGetError(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc; (void)a;
    *rv = (uint32_t)glGetError();
}
static void h_glIsTexture(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc;
    *rv = glIsTexture((GLuint)a.r[0]) ? 1 : 0;
}
static void h_glGenTextures(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    GLuint t = 0;
    glGenTextures(1, &t);
    emu::write(uc, a.r[1], &t, 4);
    *rv = 0;
}
static void h_glDeleteTextures(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    GLuint t = (GLuint)a.r[1];
    glDeleteTextures(1, &t);
    *rv = 0;
}

static void h_glTexImage2D(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    uint32_t pixels = a.arg(8);
    GLsizei w = (GLsizei)a.r[3], h = (GLsizei)a.arg(4);
    GLenum format = (GLenum)a.arg(6), type = (GLenum)a.arg(7);
    const GLvoid *p = nullptr;
    if (pixels) {
        size_t bpp = 4;
        if (type == GL_UNSIGNED_SHORT_5_6_5 || type == GL_UNSIGNED_SHORT_4_4_4_4 ||
            type == GL_UNSIGNED_SHORT_5_5_5_1)
            bpp = 2;
        else if (format == GL_RGB) bpp = 3;
        else if (format == GL_ALPHA || format == GL_LUMINANCE) bpp = 1;
        size_t n = (size_t)w * h * bpp + 32;
        p = gl_ptr(uc, pixels, n, &g_tmp_misc);
    }
    glTexImage2D((GLenum)a.r[0], (GLint)a.r[1], (GLint)a.r[2], w, h,
                 (GLint)a.arg(5), format, type, p);
    *rv = 0;
}
static void h_glCompressedTexImage2D(uc_engine *uc, const emu::Args &a,
                                     uint32_t *rv) {
    uint32_t data = a.arg(7);
    GLsizei size = (GLsizei)a.arg(6);
    const GLvoid *p = nullptr;
    if (data && size > 0) p = gl_ptr(uc, data, (size_t)size, &g_tmp_misc);
    glCompressedTexImage2D((GLenum)a.r[0], (GLint)a.r[1], (GLenum)a.r[2],
                           (GLsizei)a.r[3], (GLsizei)a.arg(4), (GLint)a.arg(5),
                           size, p);
    *rv = 0;
}
static void h_glReadPixels(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::vector<uint8_t> buf((size_t)a.r[2] * a.r[3] * 4);
    glReadPixels((GLint)a.r[0], (GLint)a.r[1], (GLsizei)a.r[2], (GLsizei)a.r[3],
                 (GLenum)a.arg(4), (GLenum)a.arg(5), buf.data());
    emu::write(uc, a.arg(6), buf.data(), buf.size());
    *rv = 0;
}
static void h_glGetFloatv(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    /* write ONLY the storage the caller provided: matrices are 16 floats,
     * everything else is 1 (GL_MAX_TEXTURE_SIZE style queries). Writing a
     * fixed 16-float block smashes the caller's stack frame. */
    GLenum pname = (GLenum)a.r[0];
    int nfloats = 1;
    if (pname == 0x0BA6 || pname == 0x0BA7 || pname == 0x0BA8 || pname == 0x0BA5)
        nfloats = 16;
    GLfloat v[16] = {0};
    glGetFloatv(pname, v);
    emu::write(uc, a.r[1], v, (size_t)nfloats * 4);
    *rv = 0;
}
static void h_glGetIntegerv(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    GLenum pname = (GLenum)a.r[0];
    int nints = 1;
    if (pname == 0x0BA2 /* GL_VIEWPORT */ || pname == 0x0C10 /* GL_SCISSOR_BOX */)
        nints = 4;
    GLint v[4] = {0};
    glGetIntegerv(pname, v);
    emu::write(uc, a.r[1], v, (size_t)nints * 4);
    *rv = 0;
}

/* ---- shader family: fails on the GLES1 context, as on the originals ---- */
static void h_shader_fail(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc; (void)a;
    *rv = 0;
}
static void h_glGetShaderiv(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    GLint v = 0;
    emu::write(uc, a.r[2], &v, 4);
    *rv = 0;
}
static void h_glGetProgramiv(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    GLint v = 0;
    emu::write(uc, a.r[2], &v, 4);
    *rv = 0;
}
static void h_glGetUniformLocation(uc_engine *uc, const emu::Args &a,
                                   uint32_t *rv) {
    (void)uc; (void)a;
    *rv = (uint32_t)-1;
}

void register_gl_imports() {
    struct { const char *n; ImportHandler f; } kTab[] = {
        /* fixed function -> real GLES1 */
        {"glMatrixMode", h_glMatrixMode}, {"glPushMatrix", h_glPushMatrix},
        {"glPopMatrix", h_glPopMatrix}, {"glLoadIdentity", h_glLoadIdentity},
        {"glLoadMatrixf", h_glLoadMatrixf}, {"glMultMatrixf", h_glMultMatrixf},
        {"glTranslatef", h_glTranslatef}, {"glScalef", h_glScalef},
        {"glRotatef", h_glRotatef}, {"glOrthof", h_glOrthof},
        {"glEnableClientState", h_glEnableClientState},
        {"glDisableClientState", h_glDisableClientState},
        {"glVertexPointer", h_glVertexPointer},
        {"glTexCoordPointer", h_glTexCoordPointer},
        {"glColorPointer", h_glColorPointer},
        {"glNormalPointer", h_glNormalPointer},
        {"glColor4f", h_glColor4f}, {"glColor4ub", h_glColor4ub},
        {"glTexEnvi", h_glTexEnvi}, {"glMaterialf", h_glMaterialf},
        {"glMaterialfv", h_glMaterialfv}, {"glLightfv", h_glLightfv},
        {"glDrawArrays", h_glDrawArrays}, {"glDrawElements", h_glDrawElements},
        {"glEnable", h_glEnable}, {"glDisable", h_glDisable},
        {"glClear", h_glClear}, {"glClearColor", h_glClearColor},
        {"glBlendFunc", h_glBlendFunc}, {"glScissor", h_glScissor},
        {"glLineWidth", h_glLineWidth}, {"glGetError", h_glGetError},
        {"glFinish", h_glFinish}, {"glIsTexture", h_glIsTexture},
        {"glGenTextures", h_glGenTextures},
        {"glDeleteTextures", h_glDeleteTextures},
        {"glBindTexture", h_glBindTexture},
        {"glTexParameteri", h_glTexParameteri},
        {"glActiveTexture", h_glActiveTexture},
        {"glClientActiveTexture", h_glClientActiveTexture},
        {"glTexImage2D", h_glTexImage2D},
        {"glCompressedTexImage2D", h_glCompressedTexImage2D},
        {"glReadPixels", h_glReadPixels},
        {"glGetFloatv", h_glGetFloatv}, {"glGetIntegerv", h_glGetIntegerv},
        {"glBindFramebufferOES", h_glBindFramebufferOES},
        /* shader API -> graceful failure (GLES1 context, like the originals) */
        {"glCreateShader", h_shader_fail}, {"glShaderSource", h_shader_fail},
        {"glCompileShader", h_shader_fail}, {"glGetShaderiv", h_glGetShaderiv},
        {"glDeleteShader", h_shader_fail}, {"glCreateProgram", h_shader_fail},
        {"glAttachShader", h_shader_fail},
        {"glBindAttribLocation", h_shader_fail},
        {"glLinkProgram", h_shader_fail}, {"glGetProgramiv", h_glGetProgramiv},
        {"glUseProgram", h_shader_fail}, {"glDeleteProgram", h_shader_fail},
        {"glGetUniformLocation", h_glGetUniformLocation},
        {"glUniform1f", h_shader_fail}, {"glUniform1i", h_shader_fail},
        {"glUniform2fv", h_shader_fail}, {"glUniform3f", h_shader_fail},
        {"glUniform3fv", h_shader_fail}, {"glUniform4f", h_shader_fail},
        {"glUniform4fv", h_shader_fail},
        {"glUniformMatrix2fv", h_shader_fail},
        {"glUniformMatrix3fv", h_shader_fail},
        {"glUniformMatrix4fv", h_shader_fail},
        {"glVertexAttribPointer", h_shader_fail},
        {"glEnableVertexAttribArray", h_shader_fail},
        {"glDisableVertexAttribArray", h_shader_fail},
        {"glVertexAttrib4f", h_shader_fail},
    };
    for (auto &e : kTab) stubs_register(e.n, e.f);
}
