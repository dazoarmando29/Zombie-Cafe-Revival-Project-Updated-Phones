/* OpenGL ES bridge: GLES2 passthrough + fixed-function shim. */
#include "imports_gl.h"

#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include <GLES2/gl2.h>
#include <android/log.h>
#include <dlfcn.h>
#include <unicorn/unicorn.h>

#include "emu_mem.h"
#include "stub_dispatch.h"

#define TAG "zcr-gl"

namespace {

/* ---------------- matrix math ---------------- */
struct Mat4 {
    float m[16];  /* column-major, GL layout */
    static Mat4 id() {
        Mat4 r;
        memset(r.m, 0, sizeof(r.m));
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.f;
        return r;
    }
    Mat4 mul(const Mat4 &o) const {
        Mat4 r;
        for (int c = 0; c < 4; c++)
            for (int rw = 0; rw < 4; rw++) {
                float s = 0;
                for (int k = 0; k < 4; k++) s += m[k * 4 + rw] * o.m[c * 4 + k];
                r.m[c * 4 + rw] = s;
            }
        return r;
    }
};

/* ---------------- fixed-function state ---------------- */
enum { M_MODELVIEW, M_PROJECTION, M_TEXTURE, M_COUNT };

struct FFState {
    int mode = M_MODELVIEW;
    Mat4 stack[M_COUNT][32];
    int top[M_COUNT] = {0, 0, 0};

    /* client arrays (emulated pointers) */
    bool va_enabled = false, tc_enabled = false, co_enabled = false, no_enabled = false;
    int va_size = 4, tc_size = 2, co_size = 4, no_size = 3;
    int va_type = GL_FLOAT, tc_type = GL_FLOAT, co_type = GL_FLOAT, no_type = GL_FLOAT;
    int va_stride = 0, tc_stride = 0, co_stride = 0, no_stride = 0;
    uint32_t va_ptr = 0, tc_ptr = 0, co_ptr = 0, no_ptr = 0;

    float color[4] = {1, 1, 1, 1};
    int tex_env = 0x2100;  /* GL_MODULATE */
    int active_tex = 0;    /* GL_TEXTURE0 */
    bool tex2d = false;
    float line_width = 1.f;
};

FFState g_ff;

/* shader program used to emulate fixed-function draws */
GLuint g_ff_prog = 0, g_ff_vs = 0, g_ff_fs = 0;
GLint g_u_mvp = -1, g_u_texmat = -1, g_u_color = -1, g_u_mode = -1, g_u_tex = -1;
GLint g_a_pos = -1, g_a_tex = -1, g_a_col = -1;

const char *kVs =
    "attribute vec3 a_pos;\n"
    "attribute vec2 a_tex;\n"
    "attribute vec4 a_col;\n"
    "uniform mat4 u_mvp;\n"
    "uniform mat4 u_texmat;\n"
    "varying vec2 v_tex;\n"
    "varying vec4 v_col;\n"
    "void main(){\n"
    "  gl_Position = u_mvp * vec4(a_pos,1.0);\n"
    "  vec4 t = u_texmat * vec4(a_tex,0.0,1.0);\n"
    "  v_tex = t.xy;\n"
    "  v_col = a_col;\n"
    "}\n";
const char *kFs =
    "precision mediump float;\n"
    "varying vec2 v_tex;\n"
    "varying vec4 v_col;\n"
    "uniform sampler2D u_tex;\n"
    "uniform int u_mode;\n"     /* 0 = no tex, 1 = modulate, 2 = replace */
    "void main(){\n"
    "  if (u_mode == 0) gl_FragColor = v_col;\n"
    "  else { vec4 t = texture2D(u_tex, v_tex);\n"
    "    gl_FragColor = (u_mode == 2) ? t : t * v_col; }\n"
    "}\n";

void ff_init_once() {
    if (g_ff_prog) return;
    auto compile = [&](GLenum type, const char *src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetShaderInfoLog(s, sizeof(log), nullptr, log);
            __android_log_print(ANDROID_LOG_ERROR, TAG, "ff shader: %s", log);
        }
        return s;
    };
    g_ff_vs = compile(GL_VERTEX_SHADER, kVs);
    g_ff_fs = compile(GL_FRAGMENT_SHADER, kFs);
    g_ff_prog = glCreateProgram();
    glAttachShader(g_ff_prog, g_ff_vs);
    glAttachShader(g_ff_prog, g_ff_fs);
    glBindAttribLocation(g_ff_prog, 0, "a_pos");
    glBindAttribLocation(g_ff_prog, 1, "a_tex");
    glBindAttribLocation(g_ff_prog, 2, "a_col");
    glLinkProgram(g_ff_prog);
    g_u_mvp = glGetUniformLocation(g_ff_prog, "u_mvp");
    g_u_texmat = glGetUniformLocation(g_ff_prog, "u_texmat");
    g_u_color = glGetUniformLocation(g_ff_prog, "u_color");
    g_u_mode = glGetUniformLocation(g_ff_prog, "u_mode");
    g_u_tex = glGetUniformLocation(g_ff_prog, "u_tex");
    g_a_pos = 0;
    g_a_tex = 1;
    g_a_col = 2;
}

/* resolve an emulated client-array pointer to a host pointer (or scratch) */
const void *client_ptr(uint32_t emu, size_t need, void *scratch) {
    if (void *p = emu::host_ptr(nullptr, emu)) return p;
    if (scratch && emu::read(nullptr, emu, scratch, need)) return scratch;
    return nullptr;
}

size_t gl_type_size(GLenum t) {
    switch (t) {
        case GL_BYTE: case GL_UNSIGNED_BYTE: return 1;
        case GL_SHORT: case GL_UNSIGNED_SHORT: return 2;
        default: return 4;
    }
}

void ff_draw(GLenum mode, GLint first, GLsizei count, GLenum itype,
             const uint16_t *indices) {
    ff_init_once();

    Mat4 mvp = g_ff.stack[M_PROJECTION][g_ff.top[M_PROJECTION]]
                   .mul(g_ff.stack[M_MODELVIEW][g_ff.top[M_MODELVIEW]]);
    Mat4 texmat = g_ff.stack[M_TEXTURE][g_ff.top[M_TEXTURE]];

    glUseProgram(g_ff_prog);
    glUniformMatrix4fv(g_u_mvp, 1, GL_FALSE, mvp.m);
    glUniformMatrix4fv(g_u_texmat, 1, GL_FALSE, texmat.m);
    glUniform4fv(g_u_color, 1, g_ff.color);
    glUniform1i(g_u_tex, 0);
    glUniform1i(g_u_mode, g_ff.tex2d ? (g_ff.tex_env == 0x1E01 ? 2 : 1) : 0);

    std::vector<uint8_t> scratch(count * 32);
    size_t per = (size_t)count * (indices ? 1 : 1);

    if (g_ff.va_enabled) {
        size_t esz = gl_type_size(g_ff.va_type);
        size_t stride = g_ff.va_stride ? g_ff.va_stride : esz * g_ff.va_size;
        const void *p = client_ptr(g_ff.va_ptr, (size_t)(first + count) * stride,
                                   scratch.data());
        if (p) {
            const uint8_t *b = (const uint8_t *)p + (size_t)first * stride;
            glVertexAttribPointer(g_a_pos, g_ff.va_size, g_ff.va_type,
                                  GL_FALSE, g_ff.va_stride, b);
            glEnableVertexAttribArray(g_a_pos);
        }
    } else {
        glVertexAttrib3f(g_a_pos, 0, 0, 0);
        glDisableVertexAttribArray(g_a_pos);
    }

    if (g_ff.tc_enabled) {
        size_t esz = gl_type_size(g_ff.tc_type);
        size_t stride = g_ff.tc_stride ? g_ff.tc_stride : esz * g_ff.tc_size;
        const void *p = client_ptr(g_ff.tc_ptr, (size_t)(first + count) * stride,
                                   scratch.data());
        if (p) {
            const uint8_t *b = (const uint8_t *)p + (size_t)first * stride;
            glVertexAttribPointer(g_a_tex, g_ff.tc_size, g_ff.tc_type,
                                  GL_FALSE, g_ff.tc_stride, b);
            glEnableVertexAttribArray(g_a_tex);
        }
    } else {
        glVertexAttrib2f(g_a_tex, 0, 0);
        glDisableVertexAttribArray(g_a_tex);
    }

    if (g_ff.co_enabled) {
        size_t esz = gl_type_size(g_ff.co_type);
        size_t stride = g_ff.co_stride ? g_ff.co_stride : esz * g_ff.co_size;
        const void *p = client_ptr(g_ff.co_ptr, (size_t)(first + count) * stride,
                                   scratch.data());
        if (p) {
            const uint8_t *b = (const uint8_t *)p + (size_t)first * stride;
            glVertexAttribPointer(g_a_col, g_ff.co_size, g_ff.co_type,
                                  g_ff.co_type == GL_UNSIGNED_BYTE ? GL_TRUE : GL_FALSE,
                                  g_ff.co_stride, b);
            glEnableVertexAttribArray(g_a_col);
        }
    } else {
        glVertexAttrib4f(g_a_col, g_ff.color[0], g_ff.color[1], g_ff.color[2],
                         g_ff.color[3]);
        glDisableVertexAttribArray(g_a_col);
    }

    if (indices) {
        std::vector<uint16_t> idx(indices, indices + count);
        glDrawElements(mode, count, itype, idx.data());
    } else {
        glDrawArrays(mode, 0, count);
    }
    (void)per;
}

bool g_shader_bound = false;

/* shader-path attribute bindings: emulated pointer + format per attrib */
struct Attrib {
    uint32_t emu = 0;
    int size = 4;
    GLenum type = GL_FLOAT;
    GLboolean norm = GL_FALSE;
    GLsizei stride = 0;
    bool enabled = false;
};
std::unordered_map<uint32_t, Attrib> g_attribs;

/* Re-resolve emulated attribute pointers into host buffers before shader
 * draws (client-side arrays; GLES2 requires valid host pointers). */
void sync_attribs(uint32_t first, uint32_t count) {
    for (auto &kv : g_attribs) {
        Attrib &at = kv.second;
        if (!at.enabled || !at.emu) continue;
        size_t esz = at.type == GL_UNSIGNED_BYTE || at.type == GL_BYTE ? 1
                   : (at.type == GL_SHORT || at.type == GL_UNSIGNED_SHORT ? 2 : 4);
        size_t stride = at.stride ? at.stride : esz * at.size;
        size_t need = (size_t)(first + count) * stride + 32;
        std::vector<uint8_t> buf(need);
        if (!emu::read(nullptr, at.emu, buf.data(), need)) continue;
        const void *p = buf.data() + (size_t)first * stride;
        glVertexAttribPointer(kv.first, at.size, at.type, at.norm, at.stride, p);
        /* GL copies client data at draw time; buffer must survive until then,
           so stash it in a static slot per attrib */
        static std::unordered_map<uint32_t, std::vector<uint8_t>> keep;
        keep[kv.first] = std::move(buf);
        glVertexAttribPointer(kv.first, at.size, at.type, at.norm, at.stride,
                              keep[kv.first].data() + (size_t)first * stride);
        glEnableVertexAttribArray(kv.first);
    }
}

/* ---------------- shared arg helpers ---------------- */
uint32_t f2u(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
float u2f(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }

}  // namespace

/* ================= handlers ================= */

static void h_glMatrixMode(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    switch (a.r[0]) {
        case 0x1700: g_ff.mode = M_MODELVIEW; break;
        case 0x1701: g_ff.mode = M_PROJECTION; break;
        default: g_ff.mode = M_TEXTURE; break;
    }
    *rv = 0;
}
static void h_glPushMatrix(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    if (g_ff.top[g_ff.mode] < 31) g_ff.top[g_ff.mode]++;
    g_ff.stack[g_ff.mode][g_ff.top[g_ff.mode]] =
        g_ff.stack[g_ff.mode][g_ff.top[g_ff.mode] - 1];
    *rv = 0;
}
static void h_glPopMatrix(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    if (g_ff.top[g_ff.mode] > 0) g_ff.top[g_ff.mode]--;
    *rv = 0;
}
static void h_glLoadIdentity(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    g_ff.stack[g_ff.mode][g_ff.top[g_ff.mode]] = Mat4::id();
    *rv = 0;
}
static void h_glLoadMatrixf(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    emu::read(uc, a.r[0], g_ff.stack[g_ff.mode][g_ff.top[g_ff.mode]].m, 64);
    *rv = 0;
}
static void h_glMultMatrixf(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    Mat4 t;
    emu::read(uc, a.r[0], t.m, 64);
    g_ff.stack[g_ff.mode][g_ff.top[g_ff.mode]] =
        g_ff.stack[g_ff.mode][g_ff.top[g_ff.mode]].mul(t);
    *rv = 0;
}
static void h_glTranslatef(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    Mat4 t = Mat4::id();
    t.m[12] = u2f(a.r[0]);
    t.m[13] = u2f(a.r[1]);
    t.m[14] = u2f(a.r[2]);
    auto &m = g_ff.stack[g_ff.mode][g_ff.top[g_ff.mode]];
    m = m.mul(t);
    *rv = 0;
}
static void h_glScalef(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    Mat4 t = Mat4::id();
    t.m[0] = u2f(a.r[0]);
    t.m[5] = u2f(a.r[1]);
    t.m[10] = u2f(a.r[2]);
    auto &m = g_ff.stack[g_ff.mode][g_ff.top[g_ff.mode]];
    m = m.mul(t);
    *rv = 0;
}
static void h_glRotatef(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    float ang = u2f(a.r[0]) * 3.14159265f / 180.f;
    float x = u2f(a.r[1]), y = u2f(a.r[2]), z = u2f(a.r[3]);
    float c = cosf(ang), s = sinf(ang), ic = 1.f - c;
    Mat4 t = Mat4::id();
    t.m[0] = x * x * ic + c;
    t.m[1] = y * x * ic + z * s;
    t.m[2] = x * z * ic - y * s;
    t.m[4] = x * y * ic - z * s;
    t.m[5] = y * y * ic + c;
    t.m[6] = y * z * ic + x * s;
    t.m[8] = x * z * ic + y * s;
    t.m[9] = y * z * ic - x * s;
    t.m[10] = z * z * ic + c;
    auto &m = g_ff.stack[g_ff.mode][g_ff.top[g_ff.mode]];
    m = m.mul(t);
    *rv = 0;
}
static void h_glOrthof(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    float l = u2f(a.r[0]), r = u2f(a.r[1]), b = u2f(a.r[2]);
    float t = u2f(a.arg(3)), n = u2f(a.arg(4)), f = u2f(a.arg(5));
    Mat4 o = Mat4::id();
    o.m[0] = 2.f / (r - l);
    o.m[5] = 2.f / (t - b);
    o.m[10] = -2.f / (f - n);
    o.m[12] = -(r + l) / (r - l);
    o.m[13] = -(t + b) / (t - b);
    o.m[14] = -(f + n) / (f - n);
    auto &m = g_ff.stack[g_ff.mode][g_ff.top[g_ff.mode]];
    m = m.mul(o);
    *rv = 0;
}

static void h_glEnableClientState(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    switch (a.r[0]) {
        case 0x8074: g_ff.va_enabled = true; break;
        case 0x8078: g_ff.tc_enabled = true; break;
        case 0x8076: g_ff.co_enabled = true; break;
        case 0x8075: g_ff.no_enabled = true; break;
    }
    *rv = 0;
}
static void h_glDisableClientState(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    switch (a.r[0]) {
        case 0x8074: g_ff.va_enabled = false; break;
        case 0x8078: g_ff.tc_enabled = false; break;
        case 0x8076: g_ff.co_enabled = false; break;
        case 0x8075: g_ff.no_enabled = false; break;
    }
    *rv = 0;
}
static void h_glVertexPointer(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    g_ff.va_size = (int)a.r[0];
    g_ff.va_type = (int)a.r[1];
    g_ff.va_stride = (int)a.r[2];
    g_ff.va_ptr = a.r[3];
    *rv = 0;
}
static void h_glTexCoordPointer(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    g_ff.tc_size = (int)a.r[0];
    g_ff.tc_type = (int)a.r[1];
    g_ff.tc_stride = (int)a.r[2];
    g_ff.tc_ptr = a.r[3];
    *rv = 0;
}
static void h_glColorPointer(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    g_ff.co_size = (int)a.r[0];
    g_ff.co_type = (int)a.r[1];
    g_ff.co_stride = (int)a.r[2];
    g_ff.co_ptr = a.r[3];
    *rv = 0;
}
static void h_glNormalPointer(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    g_ff.no_type = (int)a.r[0];
    g_ff.no_stride = (int)a.r[1];
    g_ff.no_ptr = a.r[2];
    *rv = 0;
}
static void h_glColor4f(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    g_ff.color[0] = u2f(a.r[0]);
    g_ff.color[1] = u2f(a.r[1]);
    g_ff.color[2] = u2f(a.r[2]);
    g_ff.color[3] = u2f(a.r[3]);
    *rv = 0;
}
static void h_glColor4ub(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    g_ff.color[0] = (a.r[0] & 0xFF) / 255.f;
    g_ff.color[1] = (a.r[1] & 0xFF) / 255.f;
    g_ff.color[2] = (a.r[2] & 0xFF) / 255.f;
    g_ff.color[3] = (a.r[3] & 0xFF) / 255.f;
    *rv = 0;
}
static void h_glTexEnvi(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    if (a.r[1] == 0x2200) g_ff.tex_env = (int)a.r[2];
    *rv = 0;
}
static void h_glMaterialf(uc_engine *uc, const emu::Args &a, uint32_t *rv) { *rv = 0; }
static void h_glMaterialfv(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    if (a.r[1] == 0x1200) {  /* GL_AMBIENT_AND_DIFFUSE */
        float v[4];
        emu::read(uc, a.r[2], v, 16);
        memcpy(g_ff.color, v, 16);
    }
    *rv = 0;
}
static void h_glLightfv(uc_engine *uc, const emu::Args &a, uint32_t *rv) { *rv = 0; }

static void h_glDrawArrays(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    if (g_shader_bound) {
        sync_attribs((uint32_t)a.r[1], (uint32_t)a.r[2]);
        glDrawArrays(a.r[0], (GLint)a.r[1], (GLsizei)a.r[2]);
    } else {
        ff_draw(a.r[0], (GLint)a.r[1], (GLsizei)a.r[2], GL_UNSIGNED_SHORT, nullptr);
    }
    *rv = 0;
}
static void h_glDrawElements(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    uint32_t count = a.r[1], itype = a.r[2], idx = a.r[3];
    if (g_shader_bound) {
        sync_attribs(0, count);
        std::vector<uint8_t> buf((size_t)count * (itype == GL_UNSIGNED_SHORT ? 2 : 1));
        emu::read(uc, idx, buf.data(), buf.size());
        glDrawElements(a.r[0], (GLsizei)count, itype, buf.data());
    } else if (itype == GL_UNSIGNED_SHORT) {
        std::vector<uint16_t> buf(count);
        emu::read(uc, idx, buf.data(), count * 2);
        ff_draw(a.r[0], 0, (GLsizei)count, GL_UNSIGNED_SHORT, buf.data());
    } else {
        std::vector<uint8_t> buf(count);
        emu::read(uc, idx, buf.data(), count);
        ff_draw(a.r[0], 0, (GLsizei)count, GL_UNSIGNED_BYTE,
                (const uint16_t *)buf.data());
    }
    *rv = 0;
}

/* ---- passthrough family ---- */
static void p_glEnable(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    if (a.r[0] == 0x0DE1) { g_ff.tex2d = true; *rv = 0; return; }
    glEnable(a.r[0]);
    *rv = 0;
}
static void p_glDisable(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    if (a.r[0] == 0x0DE1) { g_ff.tex2d = false; *rv = 0; return; }
    glDisable(a.r[0]);
    *rv = 0;
}

#define P0(name, glfn) \
    static void p_##name(uc_engine *uc, const emu::Args &a, uint32_t *rv) { (void)uc;(void)a; glfn; *rv = 0; }
#define P1(name, glfn) \
    static void p_##name(uc_engine *uc, const emu::Args &a, uint32_t *rv) { (void)uc; glfn(a.r[0]); *rv = 0; }
#define P2(name, glfn) \
    static void p_##name(uc_engine *uc, const emu::Args &a, uint32_t *rv) { (void)uc; glfn(a.r[0], a.r[1]); *rv = 0; }
#define P3(name, glfn) \
    static void p_##name(uc_engine *uc, const emu::Args &a, uint32_t *rv) { (void)uc; glfn(a.r[0], a.r[1], a.r[2]); *rv = 0; }
#define P4(name, glfn) \
    static void p_##name(uc_engine *uc, const emu::Args &a, uint32_t *rv) { (void)uc; glfn(a.r[0], a.r[1], a.r[2], a.r[3]); *rv = 0; }

P1(glClear, glClear)
static void p_glClearColor(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    glClearColor(u2f(a.r[0]), u2f(a.r[1]), u2f(a.r[2]), u2f(a.r[3]));
    *rv = 0;
}
P2(glBlendFunc, glBlendFunc)
P4(glScissor, glScissor)
static void p_glLineWidth(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    glLineWidth(u2f(a.r[0]));
    *rv = 0;
}
static void p_glGetError(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc; (void)a;
    *rv = glGetError();
}
P0(glFinish, glFinish)
P0(glFlush, glFlush)

static void p_glGenTextures(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    GLuint t = 0;
    glGenTextures(1, &t);
    emu::write(uc, a.r[1], &t, 4);
    *rv = 0;
}
static void p_glDeleteTextures(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    GLuint t = a.r[1];
    glDeleteTextures(1, &t);
    *rv = 0;
}
P2(glBindTexture, glBindTexture)
P3(glTexParameteri, glTexParameteri)
static void p_glIsTexture(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    *rv = glIsTexture(a.r[0]) ? 1 : 0;
}
static void p_glActiveTexture(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    g_ff.active_tex = (int)a.r[0] - 0x84C0;
    glActiveTexture(a.r[0]);
    *rv = 0;
}
static void p_glClientActiveTexture(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    *rv = 0;
}

static void p_glTexImage2D(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    /* target, level, ifmt, w, h, border, fmt, type, pixels */
    uint32_t pixels = a.arg(8);
    std::vector<uint8_t> buf;
    const void *p = nullptr;
    if (pixels) {
        size_t n = (size_t)a.r[3] * a.r[4] * 4;
        if (a.r[6] == 0x1908 || a.r[6] == 0x1907) n = (size_t)a.r[3] * a.r[4] * (a.r[6] == 0x1907 ? 3 : 4);
        if (a.r[7] == 0x8033 || a.r[7] == 0x8363) n /= 2;  /* packed types */
        if (a.r[7] == 0x8034) n = (size_t)a.r[3] * a.r[4];  /* GL_UNSIGNED_SHORT_5_6_5? keep raw */
        buf.resize(n);
        if (emu::read(uc, pixels, buf.data(), n)) p = buf.data();
    }
    glTexImage2D(a.r[0], (GLint)a.r[1], (GLint)a.r[2], (GLsizei)a.r[3],
                 (GLsizei)a.r[4], (GLint)a.r[5], (GLenum)a.r[6], (GLenum)a.r[7], p);
    *rv = 0;
}
static void p_glCompressedTexImage2D(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    uint32_t data = a.arg(7);
    std::vector<uint8_t> buf(a.arg(6));
    if (a.arg(6) && data) emu::read(uc, data, buf.data(), a.arg(6));
    glCompressedTexImage2D(a.r[0], (GLint)a.r[1], (GLenum)a.r[2], (GLsizei)a.r[3],
                           (GLsizei)a.r[4], (GLint)a.r[5], (GLsizei)a.arg(6),
                           buf.data());
    *rv = 0;
}
static void p_glReadPixels(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::vector<uint8_t> buf((size_t)a.r[2] * a.r[3] * 4);
    glReadPixels((GLint)a.r[0], (GLint)a.r[1], (GLsizei)a.r[2], (GLsizei)a.r[3],
                 (GLenum)a.r[4], (GLenum)a.r[5], buf.data());
    emu::write(uc, a.arg(6), buf.data(), buf.size());
    *rv = 0;
}

static void p_glGetFloatv(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    float v[16] = {0};
    switch (a.r[0]) {
        case 0x0BA6:  /* GL_MODELVIEW_MATRIX */
            memcpy(v, g_ff.stack[M_MODELVIEW][g_ff.top[M_MODELVIEW]].m, 64);
            emu::write(uc, a.r[1], v, 64);
            break;
        case 0x0BA7:  /* GL_PROJECTION_MATRIX */
            memcpy(v, g_ff.stack[M_PROJECTION][g_ff.top[M_PROJECTION]].m, 64);
            emu::write(uc, a.r[1], v, 64);
            break;
        case 0x0BA8:  /* GL_TEXTURE_MATRIX */
            memcpy(v, g_ff.stack[M_TEXTURE][g_ff.top[M_TEXTURE]].m, 64);
            emu::write(uc, a.r[1], v, 64);
            break;
        default: {
            GLfloat tmp[16] = {0};
            glGetFloatv(a.r[0], tmp);
            emu::write(uc, a.r[1], tmp, sizeof(tmp));
            break;
        }
    }
    *rv = 0;
}
static void p_glGetIntegerv(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    GLint tmp[4] = {0};
    glGetIntegerv(a.r[0], tmp);
    emu::write(uc, a.r[1], tmp, sizeof(tmp));
    *rv = 0;
}

/* ---- shader passthrough ---- */
static void p_glCreateShader(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    *rv = glCreateShader(a.r[0]);
}
static void p_glShaderSource(uc_engine *uc, const emu::Args & a, uint32_t *rv) {
    GLuint shader = a.r[0];
    GLsizei count = (GLsizei)a.r[1];
    uint32_t strings = a.r[2], lengths = a.r[3];
    std::vector<std::string> src(count);
    std::vector<const char *> ptrs(count);
    for (GLsizei i = 0; i < count; i++) {
        uint32_t p = 0;
        emu::read(uc, strings + i * 4, &p, 4);
        if (lengths) {
            int32_t len = 0;
            emu::read(uc, lengths + i * 4, &len, 4);
            std::vector<char> b(len);
            emu::read(uc, p, b.data(), len);
            src[i].assign(b.data(), len);
        } else {
            emu::read_str(uc, p, &src[i], 1 << 20);
        }
        ptrs[i] = src[i].c_str();
    }
    glShaderSource(shader, count, ptrs.data(), nullptr);
    *rv = 0;
}
P1(glCompileShader, glCompileShader)
static void p_glGetShaderiv(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    GLint v = 0;
    glGetShaderiv(a.r[0], a.r[1], &v);
    emu::write(uc, a.r[2], &v, 4);
    *rv = 0;
}
P1(glDeleteShader, glDeleteShader)
static void p_glCreateProgram(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc; (void)a;
    *rv = glCreateProgram();
}
P2(glAttachShader, glAttachShader)
static void p_glBindAttribLocation(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string name;
    emu::read_str(uc, a.r[2], &name);
    glBindAttribLocation(a.r[0], (GLuint)a.r[1], name.c_str());
    *rv = 0;
}
P1(glLinkProgram, glLinkProgram)
static void p_glGetProgramiv(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    GLint v = 0;
    glGetProgramiv(a.r[0], a.r[1], &v);
    emu::write(uc, a.r[2], &v, 4);
    *rv = 0;
}
static void p_glUseProgram(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    g_shader_bound = a.r[0] != 0;
    glUseProgram(a.r[0]);
    *rv = 0;
}
P1(glDeleteProgram, glDeleteProgram)
static void p_glGetUniformLocation(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string name;
    emu::read_str(uc, a.r[1], &name);
    *rv = (uint32_t)glGetUniformLocation(a.r[0], name.c_str());
}
static void p_glUniform1f(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    glUniform1f((GLint)a.r[0], u2f(a.r[1]));
    *rv = 0;
}
static void p_glUniform1i(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    glUniform1i((GLint)a.r[0], (GLint)a.r[1]);
    *rv = 0;
}
static void p_glUniform3f(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    glUniform3f((GLint)a.r[0], u2f(a.r[1]), u2f(a.r[2]), u2f(a.r[3]));
    *rv = 0;
}
static void p_glUniform4f(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    glUniform4f((GLint)a.r[0], u2f(a.r[1]), u2f(a.r[2]), u2f(a.r[3]), u2f(a.arg(4)));
    *rv = 0;
}
static void p_glUniform2fv(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::vector<float> v(2 * a.r[1]);
    emu::read(uc, a.r[2], v.data(), v.size() * 4);
    glUniform2fv((GLint)a.r[0], (GLsizei)a.r[1], v.data());
    *rv = 0;
}
static void p_glUniform3fv(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::vector<float> v(3 * a.r[1]);
    emu::read(uc, a.r[2], v.data(), v.size() * 4);
    glUniform3fv((GLint)a.r[0], (GLsizei)a.r[1], v.data());
    *rv = 0;
}
static void p_glUniform4fv(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::vector<float> v(4 * a.r[1]);
    emu::read(uc, a.r[2], v.data(), v.size() * 4);
    glUniform4fv((GLint)a.r[0], (GLsizei)a.r[1], v.data());
    *rv = 0;
}
static void p_glUniformMatrix2fv(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::vector<float> v(4 * a.r[1]);
    emu::read(uc, a.r[3], v.data(), v.size() * 4);
    glUniformMatrix2fv((GLint)a.r[0], (GLsizei)a.r[1], (GLboolean)a.r[2], v.data());
    *rv = 0;
}
static void p_glUniformMatrix3fv(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::vector<float> v(9 * a.r[1]);
    emu::read(uc, a.r[3], v.data(), v.size() * 4);
    glUniformMatrix3fv((GLint)a.r[0], (GLsizei)a.r[1], (GLboolean)a.r[2], v.data());
    *rv = 0;
}
static void p_glUniformMatrix4fv(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::vector<float> v(16 * a.r[1]);
    emu::read(uc, a.r[3], v.data(), v.size() * 4);
    glUniformMatrix4fv((GLint)a.r[0], (GLsizei)a.r[1], (GLboolean)a.r[2], v.data());
    *rv = 0;
}
static void p_glVertexAttribPointer(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    Attrib &at = g_attribs[a.r[0]];
    at.emu = a.arg(5);
    at.size = (int)a.r[1];
    at.type = (GLenum)a.r[2];
    at.norm = (GLboolean)a.r[3];
    at.stride = (GLsizei)a.r[4];
    *rv = 0;
}
static void p_glEnableVertexAttribArray(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    g_attribs[a.r[0]].enabled = true;
    *rv = 0;
}
static void p_glDisableVertexAttribArray(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    g_attribs[a.r[0]].enabled = false;
    glDisableVertexAttribArray(a.r[0]);
    *rv = 0;
}
static void p_glVertexAttrib4f(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    glVertexAttrib4f(a.r[0], u2f(a.r[1]), u2f(a.r[2]), u2f(a.r[3]), u2f(a.arg(4)));
    *rv = 0;
}
static void p_glBindFramebufferOES(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    /* engine always renders to the default framebuffer */
    *rv = 0;
}

void register_gl_imports() {
    struct { const char *n; ImportHandler f; } kTab[] = {
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
        {"glEnable", p_glEnable}, {"glDisable", p_glDisable},
        {"glClear", p_glClear}, {"glClearColor", p_glClearColor},
        {"glBlendFunc", p_glBlendFunc}, {"glScissor", p_glScissor},
        {"glLineWidth", p_glLineWidth}, {"glGetError", p_glGetError},
        {"glFinish", p_glFinish},
        {"glGenTextures", p_glGenTextures},
        {"glDeleteTextures", p_glDeleteTextures},
        {"glBindTexture", p_glBindTexture},
        {"glTexParameteri", p_glTexParameteri},
        {"glIsTexture", p_glIsTexture},
        {"glActiveTexture", p_glActiveTexture},
        {"glClientActiveTexture", p_glClientActiveTexture},
        {"glTexImage2D", p_glTexImage2D},
        {"glCompressedTexImage2D", p_glCompressedTexImage2D},
        {"glReadPixels", p_glReadPixels},
        {"glGetFloatv", p_glGetFloatv}, {"glGetIntegerv", p_glGetIntegerv},
        {"glCreateShader", p_glCreateShader}, {"glShaderSource", p_glShaderSource},
        {"glCompileShader", p_glCompileShader}, {"glGetShaderiv", p_glGetShaderiv},
        {"glDeleteShader", p_glDeleteShader}, {"glCreateProgram", p_glCreateProgram},
        {"glAttachShader", p_glAttachShader},
        {"glBindAttribLocation", p_glBindAttribLocation},
        {"glLinkProgram", p_glLinkProgram}, {"glGetProgramiv", p_glGetProgramiv},
        {"glUseProgram", p_glUseProgram}, {"glDeleteProgram", p_glDeleteProgram},
        {"glGetUniformLocation", p_glGetUniformLocation},
        {"glUniform1f", p_glUniform1f}, {"glUniform1i", p_glUniform1i},
        {"glUniform3f", p_glUniform3f}, {"glUniform4f", p_glUniform4f},
        {"glUniform2fv", p_glUniform2fv}, {"glUniform3fv", p_glUniform3fv},
        {"glUniform4fv", p_glUniform4fv},
        {"glUniformMatrix2fv", p_glUniformMatrix2fv},
        {"glUniformMatrix3fv", p_glUniformMatrix3fv},
        {"glUniformMatrix4fv", p_glUniformMatrix4fv},
        {"glVertexAttribPointer", p_glVertexAttribPointer},
        {"glEnableVertexAttribArray", p_glEnableVertexAttribArray},
        {"glDisableVertexAttribArray", p_glDisableVertexAttribArray},
        {"glVertexAttrib4f", p_glVertexAttrib4f},
        {"glBindFramebufferOES", p_glBindFramebufferOES},
    };
    for (auto &e : kTab) stubs_register(e.n, e.f);
}
