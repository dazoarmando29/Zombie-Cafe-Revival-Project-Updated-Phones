/* OpenGL ES bridge: forward to real GLES1 (the game's context type).
 *
 * The engine's fixed-function calls go straight to libGLESv1_CM. Its shader
 * calls fail against the GLES1 context exactly like on the original devices
 * (the "shader compile error" from the startup log) and the engine falls back
 * to fixed-function rendering. Pointers live in emulated memory; the heap and
 * stack are zero-copy host aliases so most client-array data resolves
 * directly, anything else is copied into scratch buffers at draw time. */
#include "imports_gl.h"

#include <algorithm>
#include <cstring>
#include <map>
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

/* ---- texture allocation tracking (Phase 5 diagnostics) ---- */
struct TexRec {
    GLsizei w = 0, h = 0;
    GLenum format = 0, type = 0;
    size_t bytes = 0;
    uint64_t hash = 0;
    uint32_t uploads = 0;
};
std::map<GLuint, TexRec> g_tex;
std::map<uint64_t, GLuint> g_tex_by_hash; /* content hash -> first live id */
uint64_t g_tex_live_bytes = 0;
uint64_t g_tex_peak_bytes = 0;
uint64_t g_tex_created = 0, g_tex_deleted = 0;
uint64_t g_tex_dupes = 0;
uint64_t g_tex_upload_bytes = 0; /* cumulative decoded bytes pushed to GL */

uint64_t fnv1a(const uint8_t *p, size_t n) {
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h ? h : 1;
}

size_t tex_bytes_for(GLsizei w, GLsizei h, GLenum format, GLenum type) {
    size_t bpp = 4;
    if (type == GL_UNSIGNED_SHORT_5_6_5 || type == GL_UNSIGNED_SHORT_4_4_4_4 ||
        type == GL_UNSIGNED_SHORT_5_5_5_1)
        bpp = 2;
    else if (format == GL_RGB) bpp = 3;
    else if (format == GL_ALPHA || format == GL_LUMINANCE) bpp = 1;
    return (size_t)w * (size_t)h * bpp;
}

void tex_track_upload(GLuint id, GLsizei w, GLsizei h, GLenum format,
                      GLenum type, size_t bytes, const void *pixels) {
    TexRec &r = g_tex[id];
    if (r.bytes) g_tex_live_bytes -= r.bytes;
    /* drop stale hash entry pointing at this id */
    if (r.hash) {
        auto it = g_tex_by_hash.find(r.hash);
        if (it != g_tex_by_hash.end() && it->second == id) g_tex_by_hash.erase(it);
    }
    r.w = w;
    r.h = h;
    r.format = format;
    r.type = type;
    r.bytes = bytes;
    r.uploads++;
    g_tex_live_bytes += bytes;
    g_tex_upload_bytes += bytes;
    if (g_tex_live_bytes > g_tex_peak_bytes) g_tex_peak_bytes = g_tex_live_bytes;
    if (pixels && bytes && bytes <= (64u << 20)) {
        /* hash only the first 8KB: full-atlas hashing cost 8MB of reads
         * per upload (load-time jank); duplicate detection still works */
        size_t hn = bytes > 8192 ? 8192 : bytes;
        r.hash = fnv1a((const uint8_t *)pixels, hn) ^
                 ((uint64_t)w << 32) ^ ((uint64_t)h << 8) ^ format;
        auto it = g_tex_by_hash.find(r.hash);
        if (it != g_tex_by_hash.end() && it->second != id) {
            g_tex_dupes++;
            if (g_tex_dupes <= 8)
                diag_log("[texdupe] id=%u duplicates id=%u %ux%u fmt=0x%x %uKB",
                         id, it->second, (int)w, (int)h, format,
                         (unsigned)(bytes / 1024));
        } else {
            g_tex_by_hash[r.hash] = id;
        }
    }
}

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
/* driver compat: mipmap min-filters without mip data make textures
 * incomplete (= white) on strict GLES drivers - coerce to GL_LINEAR. */
static void h_glTexParameteri(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc;
    GLenum target = (GLenum)a.r[0], pname = (GLenum)a.r[1];
    GLint v = (GLint)a.r[2];
    if (pname == GL_TEXTURE_MIN_FILTER &&
        (v == GL_NEAREST_MIPMAP_NEAREST || v == GL_LINEAR_MIPMAP_NEAREST ||
         v == GL_NEAREST_MIPMAP_LINEAR || v == GL_LINEAR_MIPMAP_LINEAR)) {
        static int s_coerced = 0;
        if (s_coerced++ < 16)
            diag_log("[texcompat] mipmap min-filter 0x%x -> GL_LINEAR", v);
        v = GL_LINEAR;
    }
    glTexParameteri(target, pname, v);
    *rv = 0;
}
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
    g_tex.try_emplace(t);
    g_tex_created++;
    *rv = 0;
}
static void h_glDeleteTextures(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    GLuint t = (GLuint)a.r[1];
    glDeleteTextures(1, &t);
    auto it = g_tex.find(t);
    if (it != g_tex.end()) {
        g_tex_live_bytes -= it->second.bytes;
        if (it->second.hash) {
            auto h = g_tex_by_hash.find(it->second.hash);
            if (h != g_tex_by_hash.end() && h->second == t) g_tex_by_hash.erase(h);
        }
        g_tex.erase(it);
        g_tex_deleted++;
    }
    *rv = 0;
}

/* ================= driver-compatibility layer =================
 * Strict/new GLES drivers (Android 16 era) turn rejected uploads into
 * INCOMPLETE textures, which GLES1 samples as WHITE - the "everything
 * renders white" bug. Cover the known rejects:
 *   - GL_OES_compressed_paletted_texture formats (dropped on new
 *     drivers) -> decompressed to RGBA8888 here,
 *   - invalid format/type pairs -> converted to RGBA8888 and retried,
 *   - NPOT with REPEAT wrap -> CLAMP_TO_EDGE. */

#define GL_PALETTE4_RGB8_OES 0x8B90
#define GL_PALETTE4_RGBA8_OES 0x8B91
#define GL_PALETTE4_R5_G6_B5_OES 0x8B92
#define GL_PALETTE4_RGBA4_OES 0x8B93
#define GL_PALETTE4_RGB5_A1_OES 0x8B94
#define GL_PALETTE8_RGB8_OES 0x8B95
#define GL_PALETTE8_RGBA8_OES 0x8B96
#define GL_PALETTE8_R5_G6_B5_OES 0x8B97
#define GL_PALETTE8_RGBA4_OES 0x8B98
#define GL_PALETTE8_RGB5_A1_OES 0x8B99

static void tex_compat_fixup(GLsizei w, GLsizei h) {
    if ((w & (w - 1)) != 0 || (h & (h - 1)) != 0) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
}

/* expand any 565/4444/5551/RGB/LUMINANCE/ALPHA source to RGBA8888 */
static bool tex_convert_rgba(GLsizei w, GLsizei h, GLenum format, GLenum type,
                             const uint8_t *src, size_t n,
                             std::vector<uint8_t> *out) {
    size_t count = (size_t)w * (size_t)h;
    if (!src || count == 0 || count > (64u << 20)) return false;
    out->assign(count * 4, 255);
    uint8_t *d = out->data();
    if (type == GL_UNSIGNED_BYTE) {
        if (format == GL_RGBA && n >= count * 4) {
            memcpy(d, src, count * 4);
            return true;
        }
        if (format == GL_RGB && n >= count * 3) {
            for (size_t i = 0; i < count; i++) {
                d[i * 4] = src[i * 3];
                d[i * 4 + 1] = src[i * 3 + 1];
                d[i * 4 + 2] = src[i * 3 + 2];
            }
            return true;
        }
        if ((format == GL_LUMINANCE || format == GL_ALPHA) && n >= count) {
            for (size_t i = 0; i < count; i++) {
                uint8_t v = src[i];
                if (format == GL_LUMINANCE) {
                    d[i * 4] = d[i * 4 + 1] = d[i * 4 + 2] = v;
                } else {
                    d[i * 4 + 3] = v;
                }
            }
            return true;
        }
        return false;
    }
    if (type == GL_UNSIGNED_SHORT_5_6_5 && n >= count * 2) {
        for (size_t i = 0; i < count; i++) {
            uint16_t v;
            memcpy(&v, src + i * 2, 2);
            d[i * 4] = (uint8_t)((v >> 11 & 0x1f) * 255 / 31);
            d[i * 4 + 1] = (uint8_t)((v >> 5 & 0x3f) * 255 / 63);
            d[i * 4 + 2] = (uint8_t)((v & 0x1f) * 255 / 31);
        }
        return true;
    }
    if (type == GL_UNSIGNED_SHORT_4_4_4_4 && n >= count * 2) {
        for (size_t i = 0; i < count; i++) {
            uint16_t v;
            memcpy(&v, src + i * 2, 2);
            d[i * 4] = (uint8_t)((v >> 12 & 0xf) * 17);
            d[i * 4 + 1] = (uint8_t)((v >> 8 & 0xf) * 17);
            d[i * 4 + 2] = (uint8_t)((v >> 4 & 0xf) * 17);
            d[i * 4 + 3] = (uint8_t)((v & 0xf) * 17);
        }
        return true;
    }
    if (type == GL_UNSIGNED_SHORT_5_5_5_1 && n >= count * 2) {
        for (size_t i = 0; i < count; i++) {
            uint16_t v;
            memcpy(&v, src + i * 2, 2);
            d[i * 4] = (uint8_t)((v >> 11 & 0x1f) * 255 / 31);
            d[i * 4 + 1] = (uint8_t)((v >> 6 & 0x1f) * 255 / 31);
            d[i * 4 + 2] = (uint8_t)((v >> 1 & 0x1f) * 255 / 31);
            d[i * 4 + 3] = (uint8_t)((v & 1) * 255);
        }
        return true;
    }
    return false;
}

/* GL_OES_compressed_paletted_texture -> RGBA8888 (palette table first,
 * then 4-bit or 8-bit indices). */
static bool tex_decompress_palette(GLenum internal, GLsizei w, GLsizei h,
                                   const uint8_t *src, size_t n,
                                   std::vector<uint8_t> *out) {
    size_t count = (size_t)w * (size_t)h;
    if (!src || count == 0) return false;
    int entries = 0, idx_bits = 0;
    switch (internal) {
        case GL_PALETTE4_RGB8_OES: case GL_PALETTE4_RGBA8_OES:
        case GL_PALETTE4_R5_G6_B5_OES: case GL_PALETTE4_RGBA4_OES:
        case GL_PALETTE4_RGB5_A1_OES:
            entries = 16; idx_bits = 4; break;
        case GL_PALETTE8_RGB8_OES: case GL_PALETTE8_RGBA8_OES:
        case GL_PALETTE8_R5_G6_B5_OES: case GL_PALETTE8_RGBA4_OES:
        case GL_PALETTE8_RGB5_A1_OES:
            entries = 256; idx_bits = 8; break;
        default: return false;
    }
    int ent_fmt = internal & 0xf; /* 0=RGB8 1=RGBA8 2=565 3=RGBA4 4=5551 */
    size_t ent_bytes = (ent_fmt == 1) ? 4 : (ent_fmt == 0 ? 3 : 2);
    size_t pal_bytes = (size_t)entries * ent_bytes;
    size_t idx_bytes = (idx_bits == 4) ? (count + 1) / 2 : count;
    if (n < pal_bytes + idx_bytes) return false;
    out->assign(count * 4, 255);
    uint8_t *d = out->data();
    for (size_t i = 0; i < count; i++) {
        uint8_t idx;
        if (idx_bits == 8) {
            idx = src[pal_bytes + i];
        } else {
            uint8_t b = src[pal_bytes + i / 2];
            idx = (i & 1) ? (uint8_t)(b & 0xf) : (uint8_t)(b >> 4);
        }
        const uint8_t *e = src + (size_t)idx * ent_bytes;
        switch (ent_fmt) {
            case 0: /* RGB8 */
                d[i * 4] = e[0]; d[i * 4 + 1] = e[1]; d[i * 4 + 2] = e[2];
                break;
            case 1: /* RGBA8 */
                d[i * 4] = e[0]; d[i * 4 + 1] = e[1];
                d[i * 4 + 2] = e[2]; d[i * 4 + 3] = e[3];
                break;
            case 2: { /* 565 */
                uint16_t v = (uint16_t)(e[0] | (e[1] << 8));
                d[i * 4] = (uint8_t)((v >> 11 & 0x1f) * 255 / 31);
                d[i * 4 + 1] = (uint8_t)((v >> 5 & 0x3f) * 255 / 63);
                d[i * 4 + 2] = (uint8_t)((v & 0x1f) * 255 / 31);
                break;
            }
            case 3: { /* 4444 */
                uint16_t v = (uint16_t)(e[0] | (e[1] << 8));
                d[i * 4] = (uint8_t)((v >> 12 & 0xf) * 17);
                d[i * 4 + 1] = (uint8_t)((v >> 8 & 0xf) * 17);
                d[i * 4 + 2] = (uint8_t)((v >> 4 & 0xf) * 17);
                d[i * 4 + 3] = (uint8_t)((v & 0xf) * 17);
                break;
            }
            default: { /* 5551 */
                uint16_t v = (uint16_t)(e[0] | (e[1] << 8));
                d[i * 4] = (uint8_t)((v >> 11 & 0x1f) * 255 / 31);
                d[i * 4 + 1] = (uint8_t)((v >> 6 & 0x1f) * 255 / 31);
                d[i * 4 + 2] = (uint8_t)((v >> 1 & 0x1f) * 255 / 31);
                d[i * 4 + 3] = (uint8_t)((v & 1) * 255);
                break;
            }
        }
    }
    return true;
}

/* GL_EXT_texture_compression_s3tc -> RGBA8888 (DXT1/3/5 incl. the sRGB
 * variants 0x8C00-0x8C03 found in the engine's constant pool). Drivers
 * without S3TC reject these uploads and the texture samples WHITE - one
 * of the white-screen doors on new Android GPUs. */
static void dxt_color_block(const uint8_t *blk, uint8_t *out4x4,
                            bool force4) {
    uint16_t c0 = (uint16_t)(blk[0] | (blk[1] << 8));
    uint16_t c1 = (uint16_t)(blk[2] | (blk[3] << 8));
    uint32_t rgb[4][3];
    for (int i = 0; i < 2; i++) {
        uint16_t c = i ? c1 : c0;
        rgb[i][0] = (uint32_t)(((c >> 11) & 31) * 255 / 31);
        rgb[i][1] = (uint32_t)(((c >> 5) & 63) * 255 / 63);
        rgb[i][2] = (uint32_t)((c & 31) * 255 / 31);
    }
    bool four = force4 || (c0 > c1);
    if (four) {
        for (int k = 0; k < 3; k++) {
            rgb[2][k] = (2u * rgb[0][k] + rgb[1][k]) / 3u;
            rgb[3][k] = (rgb[0][k] + 2u * rgb[1][k]) / 3u;
        }
    } else {
        for (int k = 0; k < 3; k++) rgb[2][k] = (rgb[0][k] + rgb[1][k]) / 2u;
        rgb[3][0] = rgb[3][1] = rgb[3][2] = 0;
    }
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int idx = (blk[4 + y] >> (x * 2)) & 3;
            uint8_t *o = out4x4 + (y * 4 + x) * 4;
            o[0] = (uint8_t)rgb[idx][0];
            o[1] = (uint8_t)rgb[idx][1];
            o[2] = (uint8_t)rgb[idx][2];
            o[3] = (uint8_t)((!four && idx == 3) ? 0 : 255);
        }
    }
}

static bool tex_decompress_dxt(GLenum internal, GLsizei w, GLsizei h,
                               const uint8_t *src, size_t n,
                               std::vector<uint8_t> *out) {
    int kind = 0; /* 1=DXT1, 3=DXT3, 5=DXT5 */
    switch (internal) {
        case 0x83F0: case 0x8C00: case 0x8C01: kind = 1; break;
        case 0x83F2: case 0x8C02: kind = 3; break;
        case 0x83F3: case 0x8C03: kind = 5; break;
        default: return false;
    }
    size_t block = (kind == 1) ? 8 : 16;
    size_t need = (size_t)((w + 3) / 4) * (size_t)((h + 3) / 4) * block;
    if (!src || w <= 0 || h <= 0 || n < need) return false;
    out->assign((size_t)w * (size_t)h * 4, 255);
    uint8_t *d = out->data();
    size_t off = 0;
    for (int by = 0; by < (h + 3) / 4; by++) {
        for (int bx = 0; bx < (w + 3) / 4; bx++) {
            const uint8_t *blk = src + off;
            off += block;
            uint8_t a[16];
            for (int i = 0; i < 16; i++) a[i] = 255;
            const uint8_t *color = blk;
            if (kind == 3) {
                for (int i = 0; i < 16; i++) {
                    int nib = (blk[i / 2] >> ((i % 2) * 4)) & 0xf;
                    a[i] = (uint8_t)(nib * 17);
                }
                color = blk + 8;
            } else if (kind == 5) {
                uint8_t a0 = blk[0], a1 = blk[1];
                uint8_t pal[8];
                pal[0] = a0;
                pal[1] = a1;
                if (a0 > a1) {
                    for (int i = 1; i <= 6; i++)
                        pal[1 + i] = (uint8_t)(((7 - i) * a0 + i * a1) / 7);
                } else {
                    for (int i = 1; i <= 4; i++)
                        pal[1 + i] = (uint8_t)(((5 - i) * a0 + i * a1) / 5);
                    pal[6] = 0;
                    pal[7] = 255;
                }
                uint64_t bits = (uint64_t)(blk[2] | (blk[3] << 8) |
                                           (blk[4] << 16)) |
                                ((uint64_t)(blk[5] | (blk[6] << 8) |
                                            (blk[7] << 16)) << 24);
                for (int i = 0; i < 16; i++)
                    a[i] = pal[(bits >> (3 * i)) & 7];
                color = blk + 8;
            }
            uint8_t px[64];
            dxt_color_block(color, px, kind != 1);
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int gx = bx * 4 + x, gy = by * 4 + y;
                    if (gx >= w || gy >= h) continue;
                    uint8_t *o = d + ((size_t)gy * w + gx) * 4;
                    const uint8_t *p = px + (y * 4 + x) * 4;
                    o[0] = p[0];
                    o[1] = p[1];
                    o[2] = p[2];
                    o[3] = (kind == 1) ? p[3] : a[y * 4 + x];
                }
            }
        }
    }
    return true;
}

/* Last resort: a COMPLETE 1x1 gray texture. An incomplete texture samples
 * as WHITE on GLES1 (the white-ghost bug) - never leave one incomplete. */
static void tex_upload_fallback(GLenum target, GLint level) {
    static const uint8_t kGray[4] = {128, 128, 128, 255};
    glTexImage2D(target, level, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 kGray);
}

static void h_glTexImage2D(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    uint32_t pixels = a.arg(8);
    GLsizei w = (GLsizei)a.r[3], h = (GLsizei)a.arg(4);
    GLenum format = (GLenum)a.arg(6), type = (GLenum)a.arg(7);
    const GLvoid *p = nullptr;
    size_t n = tex_bytes_for(w, h, format, type);
    if (pixels) {
        p = gl_ptr(uc, pixels, n + 32, &g_tmp_misc);
    }
    glTexImage2D((GLenum)a.r[0], (GLint)a.r[1], (GLint)a.r[2], w, h,
                 (GLint)a.arg(5), format, type, p);
    /* driver compat: retry converted when the upload was rejected */
    GLenum err = glGetError();
    if (err != GL_NO_ERROR && p && w > 0 && h > 0) {
        static int s_retries = 0;
        std::vector<uint8_t> conv;
        bool ok = tex_convert_rgba(w, h, format, type, (const uint8_t *)p, n,
                                   &conv);
        if (ok) {
            glTexImage2D((GLenum)a.r[0], (GLint)a.r[1], GL_RGBA, w, h, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, conv.data());
            err = glGetError();
        }
        if (!ok || err != GL_NO_ERROR) {
            /* never leave the texture incomplete (samples WHITE) */
            tex_upload_fallback((GLenum)a.r[0], (GLint)a.r[1]);
            if (s_retries++ < 24)
                diag_log("[texcompat] texImage2D FALLBACK f=0x%x t=0x%x "
                         "%dx%d err=0x%x", format, type, w, h, err);
        } else if (s_retries++ < 24) {
            diag_log("[texcompat] texImage2D f=0x%x t=0x%x %dx%d "
                     "err=0x%x -> RGBA8888 retry",
                     format, type, w, h, err);
        }
    }
    tex_compat_fixup(w, h);
    /* track on the 2D base level only (mip uploads replace the estimate) */
    if ((GLenum)a.r[0] == GL_TEXTURE_2D && (GLint)a.r[1] == 0) {
        GLuint bound = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint *)&bound);
        if (bound) tex_track_upload(bound, w, h, format, type, n, p);
    }
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
    /* driver compat: paletted/S3TC formats are gone or flaky on new
     * drivers - decompress to RGBA8888 ourselves */
    GLenum err = glGetError();
    if (err != GL_NO_ERROR && p && size > 0) {
        GLsizei w = (GLsizei)a.r[3], h = (GLsizei)a.arg(4);
        std::vector<uint8_t> conv;
        bool ok = tex_decompress_palette((GLenum)a.r[2], w, h,
                                         (const uint8_t *)p, (size_t)size,
                                         &conv) ||
                  tex_decompress_dxt((GLenum)a.r[2], w, h,
                                     (const uint8_t *)p, (size_t)size, &conv);
        if (ok) {
            glTexImage2D((GLenum)a.r[0], (GLint)a.r[1], GL_RGBA, w, h, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, conv.data());
            err = glGetError();
        }
        if (!ok || err != GL_NO_ERROR) {
            tex_upload_fallback((GLenum)a.r[0], (GLint)a.r[1]);
            static int s_pal = 0;
            if (s_pal++ < 24)
                diag_log("[texcompat] compressed FALLBACK fmt=0x%x %dx%d "
                         "err=0x%x", (unsigned)a.r[2], (int)a.r[3],
                         (int)a.arg(4), err);
        } else {
            static int s_pal = 0;
            if (s_pal++ < 24)
                diag_log("[texcompat] compressed 0x%x %dx%d err=0x%x -> "
                         "RGBA8888", (unsigned)a.r[2], w, h, err);
        }
    }
    tex_compat_fixup((GLsizei)a.r[3], (GLsizei)a.arg(4));
    if ((GLenum)a.r[0] == GL_TEXTURE_2D && (GLint)a.r[1] == 0) {
        GLuint bound = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint *)&bound);
        if (bound)
            tex_track_upload(bound, (GLsizei)a.r[3], (GLsizei)a.arg(4),
                             (GLenum)a.r[2], 0, (size_t)size, p);
    }
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

void gl_dump_tex_stats() {
    diag_log("[texstat] live=%u peakKB=%llu liveKB=%llu created=%llu deleted=%llu dupes=%llu uploadedKB=%llu",
             (unsigned)g_tex.size(),
             (unsigned long long)(g_tex_peak_bytes / 1024),
             (unsigned long long)(g_tex_live_bytes / 1024),
             (unsigned long long)g_tex_created,
             (unsigned long long)g_tex_deleted,
             (unsigned long long)g_tex_dupes,
             (unsigned long long)(g_tex_upload_bytes / 1024));
    std::vector<std::pair<GLuint, TexRec>> top(g_tex.begin(), g_tex.end());
    std::sort(top.begin(), top.end(),
              [](const auto &x, const auto &y) { return x.second.bytes > y.second.bytes; });
    for (size_t i = 0; i < top.size() && i < 10; i++) {
        const TexRec &r = top[i].second;
        diag_log("[texstat] id=%u %dx%d fmt=0x%x type=0x%x %uKB uploads=%u",
                 top[i].first, (int)r.w, (int)r.h, r.format, r.type,
                 (unsigned)(r.bytes / 1024), r.uploads);
    }
}
