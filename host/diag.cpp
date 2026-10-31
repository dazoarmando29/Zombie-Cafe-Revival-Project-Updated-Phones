#include "diag.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <android/log.h>

#define TAG "zcr"

uint32_t g_diag_unimpl = 0;
uint32_t g_diag_emu_errors = 0;
uint32_t g_diag_draws = 0;

namespace {

std::mutex g_mu;
FILE *g_file = nullptr;
std::vector<std::string> g_unimpl_names;

FILE *log_file() {
    if (g_file) return g_file;
    static const char *kPaths[] = {
        "/sdcard/Download/zcr-log.txt",
        "/storage/emulated/0/Download/zcr-log.txt",
        "/sdcard/zcr-log.txt",
        "/storage/emulated/0/zcr-log.txt",
    };
    for (const char *p : kPaths) {
        FILE *f = fopen(p, "a");
        if (f) {
            g_file = f;
            return f;
        }
    }
    return nullptr;
}

}  // namespace

void diag_log(const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    __android_log_print(ANDROID_LOG_INFO, TAG, "%s", buf);
    std::lock_guard<std::mutex> lk(g_mu);
    if (FILE *f = log_file()) {
        fputs(buf, f);
        fputc('\n', f);
        fflush(f);
    }
}

void diag_note_unimpl(const char *name) {
    g_diag_unimpl++;
    bool known = false;
    for (const auto &s : g_unimpl_names)
        if (s == name) { known = true; break; }
    if (!known) {
        g_unimpl_names.push_back(name);
        diag_log("[unimpl] %s", name);
    }
}

void diag_note_emu_error(const char *where, const char *what) {
    g_diag_emu_errors++;
    diag_log("[emu-error] %s: %s", where, what);
}

void diag_note_draw() { g_diag_draws++; }

/* ---- trace ring ---- */
namespace {
constexpr int kTraceN = 48;
std::string g_traces[kTraceN];
int g_trace_w = 0;
}  // namespace

void diag_trace(const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    /* collapse identical consecutive calls (volume/GetDate spam) */
    if (g_trace_w > 0) {
        std::string &last = g_traces[(g_trace_w - 1) % kTraceN];
        if (last == buf) return;
    }
    g_traces[g_trace_w % kTraceN] = buf;
    g_trace_w++;
    /* live too (filterable); bounded by logd rate limits */
    __android_log_print(ANDROID_LOG_INFO, "zcr", "[trace] %s", buf);
}

void diag_dump_traces() {
    diag_log("[trace] ---- last %d calls before fault ----", kTraceN);
    int start = g_trace_w > kTraceN ? g_trace_w - kTraceN : 0;
    for (int i = start; i < g_trace_w; i++)
        diag_log("[trace] %s", g_traces[i % kTraceN].c_str());
}

void diag_summary() {
    diag_log("[summary] draws=%u unimpl=%u emu_errors=%u", g_diag_draws,
             g_diag_unimpl, g_diag_emu_errors);
    std::lock_guard<std::mutex> lk(g_mu);
    for (const auto &s : g_unimpl_names) diag_log("[unimpl-list] %s", s.c_str());
}
