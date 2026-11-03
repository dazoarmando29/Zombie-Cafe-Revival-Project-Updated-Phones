#include "diag.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <android/log.h>

#include "emu_heap.h"
#include "emu_mem.h"
#include "imports_gl.h"

#define TAG "zcr"

uint32_t g_diag_unimpl = 0;
uint32_t g_diag_emu_errors = 0;
uint32_t g_diag_draws = 0;

namespace {

std::mutex g_mu;
FILE *g_file = nullptr;
std::string g_log_dir;
std::string g_log_path;
long g_written = 0;
std::vector<std::string> g_unimpl_names;

FILE *log_file() {
    if (g_file) return g_file;
    /* NOTE: always called with g_mu held (from diag_log) */
    std::vector<std::string> paths;
    if (!g_log_dir.empty()) paths.push_back(g_log_dir + "/zcr-log.txt");
    /* legacy world-readable locations (pre-scoped-storage devices) */
    paths.push_back("/sdcard/Download/zcr-log.txt");
    paths.push_back("/storage/emulated/0/Download/zcr-log.txt");
    paths.push_back("/sdcard/zcr-log.txt");
    paths.push_back("/storage/emulated/0/zcr-log.txt");
    for (const std::string &p : paths) {
        FILE *f = fopen(p.c_str(), "a");
        if (f) {
            g_file = f;
            g_log_path = p;
            fseek(f, 0, SEEK_END);
            g_written = ftell(f);
            fprintf(f, "[diag] logging to %s\n", p.c_str());
            fflush(f);
            return f;
        }
    }
    return nullptr;
}

}  // namespace

void diag_set_log_dir(const char *dir) {
    std::lock_guard<std::mutex> lk(g_mu);
    if (dir && *dir) g_log_dir = dir;
}

bool diag_verbose() {
    static int v = -1;
    if (v < 0) {
        v = 0;
        if (!g_log_dir.empty()) {
            FILE *f = fopen((g_log_dir + "/zcr_verbose").c_str(), "r");
            if (f) {
                v = 1;
                fclose(f);
            }
        }
    }
    return v == 1;
}

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
        g_written += (long)strlen(buf) + 1;
        /* rotate at 8MB: an unbounded log turns into 100s of MB of
         * per-call I/O on long sessions (a real jank source) */
        if (g_written > (8L << 20) && !g_log_path.empty()) {
            fclose(f);
            g_file = nullptr;
            remove((g_log_path + ".1").c_str());
            rename(g_log_path.c_str(), (g_log_path + ".1").c_str());
            g_written = 0;
        }
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

void diag_report(const char *why) {
    diag_log("[report] ==== memory report (%s) ====", why ? why : "?");
    emu::heap_dump_stats();
    gl_dump_tex_stats();
    /* host process memory (RSS/VMS) where /proc is readable */
    FILE *f = fopen("/proc/self/status", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (!strncmp(line, "VmRSS:", 6) || !strncmp(line, "VmSize:", 7) ||
                !strncmp(line, "VmPeak:", 7) || !strncmp(line, "VmHWM:", 6))
                diag_log("[report] %s", line);
        }
        fclose(f);
    }
    diag_summary();
    diag_log("[report] ==== end report ====");
}
