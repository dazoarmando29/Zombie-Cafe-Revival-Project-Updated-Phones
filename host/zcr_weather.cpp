#include "zcr_weather.h"

#include <GLES/gl.h>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace zcr_weather {
namespace {

struct Particle {
    float x, y;    /* normalized 0..1 */
    float vx, vy;  /* per second */
    float size;    /* px at 1260px-tall reference */
    float alpha;
    float phase;
    uint32_t color; /* RGB8 */
};

constexpr int kMax = 520;
Particle g_p[kMax];
int g_count = 0;
int g_cur_event = -1;
float g_time = 0.0f;

uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

float frand() { return (float)rand() / (float)RAND_MAX; }

void seed_particle(Particle *p, int ev, bool top) {
    switch (ev) {
        case 2: /* Christmas snow */
            p->x = frand();
            p->y = top ? 1.02f + frand() * 0.15f : frand() * 1.1f - 0.05f;
            p->vx = (frand() - 0.5f) * 0.02f;
            p->vy = -(0.045f + frand() * 0.11f);
            p->size = 1.6f + frand() * 3.2f;
            if (frand() < 0.12f) p->size = 5.0f + frand() * 2.5f; /* big flakes */
            p->alpha = 0.35f + frand() * 0.6f;
            p->color = frand() < 0.25f ? rgb(200, 220, 255) : rgb(255, 255, 255);
            break;
        case 1: /* Halloween embers/sparks */
            p->x = frand();
            p->y = top ? -0.04f : frand();
            p->vx = (frand() - 0.5f) * 0.018f;
            p->vy = 0.02f + frand() * 0.055f;
            p->size = 1.4f + frand() * 2.4f;
            p->alpha = 0.25f + frand() * 0.5f;
            p->color = frand() < 0.55f ? rgb(255, 140, 30) : rgb(190, 80, 255);
            break;
        case 3: /* Easter petals */
            p->x = frand();
            p->y = top ? 1.02f + frand() * 0.12f : frand() * 1.1f - 0.05f;
            p->vx = (frand() - 0.5f) * 0.05f;
            p->vy = -(0.03f + frand() * 0.07f);
            p->size = 2.0f + frand() * 3.0f;
            p->alpha = 0.35f + frand() * 0.45f;
            {
                static const uint32_t kPastel[4] = {
                    rgb(255, 170, 210), rgb(170, 230, 190),
                    rgb(210, 180, 255), rgb(255, 230, 170)};
                p->color = kPastel[rand() & 3];
            }
            break;
        default:
            p->alpha = 0;
            break;
    }
    p->phase = frand() * 6.2831853f;
}

void rebuild(int ev) {
    g_cur_event = ev;
    g_count = (ev == 2) ? 480 : (ev == 1 ? 240 : (ev == 3 ? 260 : 0));
    for (int i = 0; i < g_count; i++) seed_particle(&g_p[i], ev, false);
}

void tint_for(int ev, float *r, float *g, float *b, float *a) {
    switch (ev) {
        case 2: *r = 0.55f; *g = 0.66f; *b = 0.95f; *a = 0.10f; break; /* icy */
        case 1: *r = 0.30f; *g = 0.06f; *b = 0.34f; *a = 0.13f; break; /* spooky */
        case 3: *r = 0.96f; *g = 0.72f; *b = 0.92f; *a = 0.09f; break; /* pastel */
        default: *a = 0; break;
    }
}

}  // namespace

void on_frame(int event_id, float delta_s) {
    if (event_id <= 0) {
        g_cur_event = 0;
        g_count = 0;
        return;
    }
    if (delta_s < 0.0f) delta_s = 0.0f;
    if (delta_s > 0.1f) delta_s = 0.1f;
    g_time += delta_s;
    if (event_id != g_cur_event) rebuild(event_id);

    GLint vp[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, vp);
    float vh = vp[3] > 0 ? (float)vp[3] : 1260.0f;
    float scale = vh / 1260.0f;

    /* physics */
    for (int i = 0; i < g_count; i++) {
        Particle *p = &g_p[i];
        float sway = (event_id == 1) ? 0.004f : 0.018f;
        p->x += (p->vx + sway * sinf(g_time * 1.4f + p->phase)) * delta_s;
        p->y += p->vy * delta_s;
        if (event_id == 3) p->x += 0.02f * sinf(g_time * 2.1f + p->phase) * delta_s;
        if (p->y < -0.08f || p->y > 1.12f || p->x < -0.08f || p->x > 1.08f)
            seed_particle(p, event_id, true);
    }

    /* draw: SAVE the pipeline state we touch - the engine assumes its
     * client arrays and enables persist across frames (2026-11-04: a
     * bare glDisableClientState(GL_VERTEX_ARRAY) here blanked the whole
     * game render on the next frame). */
    GLboolean en_tex = glIsEnabled(GL_TEXTURE_2D);
    GLboolean en_depth = glIsEnabled(GL_DEPTH_TEST);
    GLboolean en_cull = glIsEnabled(GL_CULL_FACE);
    GLboolean en_light = glIsEnabled(GL_LIGHTING);
    GLboolean en_blend = glIsEnabled(GL_BLEND);
    GLboolean en_vtx = glIsEnabled(GL_VERTEX_ARRAY);
    GLboolean en_col = glIsEnabled(GL_COLOR_ARRAY);
    GLint prev_mode = GL_MODELVIEW;
    glGetIntegerv(GL_MATRIX_MODE, &prev_mode);
    GLint prev_blend_src = 0, prev_blend_dst = 0;
    glGetIntegerv(0x0BE1, &prev_blend_src); /* GL_BLEND_SRC */
    glGetIntegerv(0x0BE2, &prev_blend_dst); /* GL_BLEND_DST */

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrthof(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnableClientState(GL_VERTEX_ARRAY);

    /* theme tint (full-screen quad) */
    float tr, tg, tb, ta;
    tint_for(event_id, &tr, &tg, &tb, &ta);
    if (ta > 0.0f) {
        static const GLfloat quad[8] = {0, 0, 1, 0, 0, 1, 1, 1};
        glColor4f(tr, tg, tb, ta);
        glVertexPointer(2, GL_FLOAT, 0, quad);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    /* particles as points, 3 fixed size buckets (plain GLES1 only) */
    static GLfloat s_pts[3][kMax * 2];
    static GLfloat s_col[3][kMax * 4];
    static int s_n[3];
    memset(s_n, 0, sizeof(s_n));
    static const float kSizes[3] = {2.2f, 4.0f, 6.5f};
    for (int i = 0; i < g_count; i++) {
        const Particle *p = &g_p[i];
        int cls = (p->size < 3.0f) ? 0 : (p->size < 5.0f ? 1 : 2);
        int n = s_n[cls]++;
        s_pts[cls][n * 2] = p->x;
        s_pts[cls][n * 2 + 1] = p->y;
        float tw = 1.0f;
        if (event_id == 1) /* halloween flicker */
            tw = 0.55f + 0.45f * sinf(g_time * 7.0f + p->phase * 3.0f);
        s_col[cls][n * 4] = ((p->color >> 16) & 0xff) / 255.0f;
        s_col[cls][n * 4 + 1] = ((p->color >> 8) & 0xff) / 255.0f;
        s_col[cls][n * 4 + 2] = (p->color & 0xff) / 255.0f;
        s_col[cls][n * 4 + 3] = p->alpha * tw;
    }
    glEnableClientState(GL_COLOR_ARRAY);
    for (int cls = 0; cls < 3; cls++) {
        if (!s_n[cls]) continue;
        glPointSize(kSizes[cls] * scale);
        glColorPointer(4, GL_FLOAT, 0, s_col[cls]);
        glVertexPointer(2, GL_FLOAT, 0, s_pts[cls]);
        glDrawArrays(GL_POINTS, 0, s_n[cls]);
    }
    glDisableClientState(GL_COLOR_ARRAY);

    /* restore EVERYTHING we touched */
    if (!en_vtx) glDisableClientState(GL_VERTEX_ARRAY);
    if (!en_col) glDisableClientState(GL_COLOR_ARRAY);
    else glEnableClientState(GL_COLOR_ARRAY);
    if (en_tex) glEnable(GL_TEXTURE_2D);
    else glDisable(GL_TEXTURE_2D);
    if (en_depth) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);
    if (en_cull) glEnable(GL_CULL_FACE);
    else glDisable(GL_CULL_FACE);
    if (en_light) glEnable(GL_LIGHTING);
    else glDisable(GL_LIGHTING);
    if (en_blend) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
    glBlendFunc((GLenum)prev_blend_src, (GLenum)prev_blend_dst);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode((GLenum)prev_mode);
}

}  // namespace zcr_weather
