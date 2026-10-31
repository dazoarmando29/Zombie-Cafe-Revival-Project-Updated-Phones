/* Host-backed implementations of the engine's libc/zlib/stdio/math imports. */
#include "imports_posix.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <unistd.h>
#include <zlib.h>

#include <android/log.h>
#include <unicorn/unicorn.h>

#include "engine_memory.h"
#include "emu_mem.h"
#include "stub_dispatch.h"

#define TAG "zcr-engine"

namespace {

/* ---- FILE* handle table (emulated addr -> host FILE*) ---- */
std::mutex g_io_mu;
std::map<uint32_t, FILE *> g_files;
std::map<FILE *, uint32_t> g_file_addr;
uint32_t g_file_next = 0xF5000000;

uint32_t file_token(FILE *f) {
    std::lock_guard<std::mutex> lk(g_io_mu);
    auto it = g_file_addr.find(f);
    if (it != g_file_addr.end()) return it->second;
    uint32_t t = g_file_next;
    g_file_next += 16;
    g_files[t] = f;
    g_file_addr[f] = t;
    return t;
}
FILE *file_get(uint32_t tok) {
    std::lock_guard<std::mutex> lk(g_io_mu);
    auto it = g_files.find(tok);
    return it == g_files.end() ? nullptr : it->second;
}

/* ---- minimal variadic printf over emulated args ---- */
struct Fmt {
    uc_engine *uc;
    emu::Args args;
    unsigned next;      /* next arg index (r-index based) */
    bool align_done = false;

    uint32_t word() {
        /* doubles are 8-byte aligned in the vararg stream */
        return args.arg(next++);
    }
    double dbl() {
        if (next & 1) next++;   /* align to even word */
        uint32_t lo = args.arg(next++);
        uint32_t hi = args.arg(next++);
        uint64_t u = ((uint64_t)hi << 32) | lo;
        double d;
        memcpy(&d, &u, 8);
        return d;
    }
    std::string str() {
        std::string s;
        emu::read_str(uc, word(), &s);
        return s;
    }
};

void format_into(uc_engine *uc, const emu::Args &args, unsigned first_arg,
                 const std::string &fmt, std::string *out) {
    Fmt f{uc, args, first_arg};
    out->clear();
    size_t i = 0;
    char piece[512];
    while (i < fmt.size()) {
        char c = fmt[i];
        if (c != '%') {
            *out += c;
            i++;
            continue;
        }
        size_t j = i + 1;
        while (j < fmt.size() && strchr("-+ #0123456789.*hlLzj", fmt[j])) j++;
        if (j >= fmt.size()) break;
        char type = fmt[j];
        std::string spec = fmt.substr(i, j - i + 1);
        i = j + 1;
        switch (type) {
            case '%':
                *out += '%';
                break;
            case 'd': case 'i': {
                if (spec.find("ll") != std::string::npos) {
                    long long v = (long long)(((unsigned long long)f.word() << 32) | f.word());
                    snprintf(piece, sizeof(piece), spec.c_str(), v);
                } else if (spec.find('l') != std::string::npos) {
                    snprintf(piece, sizeof(piece), spec.c_str(), (long)f.word());
                } else {
                    snprintf(piece, sizeof(piece), spec.c_str(), (int)f.word());
                }
                *out += piece;
                break;
            }
            case 'u': case 'x': case 'X': case 'o': {
                if (spec.find("ll") != std::string::npos) {
                    unsigned long long v = ((unsigned long long)f.word() << 32) | f.word();
                    snprintf(piece, sizeof(piece), spec.c_str(), v);
                } else {
                    snprintf(piece, sizeof(piece), spec.c_str(), (unsigned)f.word());
                }
                *out += piece;
                break;
            }
            case 'f': case 'g': case 'e': case 'G': case 'E':
                snprintf(piece, sizeof(piece), spec.c_str(), f.dbl());
                *out += piece;
                break;
            case 's': {
                std::string s = f.str();
                snprintf(piece, sizeof(piece), spec.c_str(), s.c_str());
                *out += piece;
                break;
            }
            case 'c':
                *out += (char)f.word();
                break;
            case 'p':
                snprintf(piece, sizeof(piece), spec.c_str(), (void *)(uintptr_t)f.word());
                *out += piece;
                break;
            default:
                *out += spec;
                break;
        }
    }
}

/* ---- zlib stream side table (emulated z_stream -> host z_stream) ---- */
std::mutex g_zmu;
std::map<uint32_t, z_stream *> g_zstreams;

z_stream *zget(uc_engine *uc, uint32_t addr) {
    std::lock_guard<std::mutex> lk(g_zmu);
    auto it = g_zstreams.find(addr);
    return it == g_zstreams.end() ? nullptr : it->second;
}

/* Write stream progress back in EMULATED pointer space. Host buffer
 * pointers must never leak into the engine's z_stream - libpng/deflate
 * call inflate() repeatedly and continue from next_in/next_out. */
void zwrite_back(uc_engine *uc, uint32_t addr, z_stream *z,
                 uint32_t in_ptr, uint32_t in_len, uint32_t out_ptr,
                 uint32_t out_len) {
    uint32_t v;
    uint32_t in_used = in_len - z->avail_in;
    uint32_t out_used = out_len - z->avail_out;
    v = in_ptr + in_used;        emu::write(uc, addr + 0, &v, 4);   /* next_in  */
    v = z->avail_in;             emu::write(uc, addr + 4, &v, 4);   /* avail_in */
    v = (uint32_t)z->total_in;   emu::write(uc, addr + 8, &v, 4);
    v = out_ptr + out_used;      emu::write(uc, addr + 12, &v, 4);  /* next_out */
    v = z->avail_out;            emu::write(uc, addr + 16, &v, 4);  /* avail_out*/
    v = (uint32_t)z->total_out;  emu::write(uc, addr + 20, &v, 4);
}

}  // namespace

/* ================= handlers ================= */

static void h_memcpy(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    uint32_t dst = a.r[0], src = a.r[1], n = a.r[2];
    void *hd = emu::host_ptr(uc, dst), *hs = emu::host_ptr(uc, src);
    if (hd && hs) memcpy(hd, hs, n);
    else {
        std::vector<uint8_t> buf(n);
        emu::read(uc, src, buf.data(), n);
        emu::write(uc, dst, buf.data(), n);
    }
    *rv = dst;
}
static void h_memset(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    uint32_t dst = a.r[0], n = a.r[2];
    if (void *hd = emu::host_ptr(uc, dst)) memset(hd, a.r[1] & 0xFF, n);
    else {
        std::vector<uint8_t> buf(n, a.r[1] & 0xFF);
        emu::write(uc, dst, buf.data(), n);
    }
    *rv = dst;
}
static void h_memmove(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    uint32_t dst = a.r[0], src = a.r[1], n = a.r[2];
    void *hd = emu::host_ptr(uc, dst), *hs = emu::host_ptr(uc, src);
    if (hd && hs) memmove(hd, hs, n);
    else {
        std::vector<uint8_t> buf(n);
        emu::read(uc, src, buf.data(), n);
        emu::write(uc, dst, buf.data(), n);
    }
    *rv = dst;
}
static void h_memcmp(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string x, y;
    emu::read_str(uc, a.r[0], &x, a.r[2] ? a.r[2] : 1);
    emu::read_str(uc, a.r[1], &y, a.r[2] ? a.r[2] : 1);
    std::vector<uint8_t> bx(a.r[2]), by(a.r[2]);
    emu::read(uc, a.r[0], bx.data(), a.r[2]);
    emu::read(uc, a.r[1], by.data(), a.r[2]);
    *rv = (uint32_t)memcmp(bx.data(), by.data(), a.r[2]);
}

#define STR1(name, expr)                                            \
    static void h_##name(uc_engine *uc, const emu::Args &a,         \
                         uint32_t *rv) {                            \
        (void)uc; (void)a; (void)rv; expr;                          \
    }

static void h_strlen(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string s; emu::read_str(uc, a.r[0], &s); *rv = (uint32_t)s.size();
}
static void h_strcpy(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string s; emu::read_str(uc, a.r[1], &s);
    emu::write(uc, a.r[0], s.c_str(), s.size() + 1);
    *rv = a.r[0];
}
static void h_strncpy(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string s; emu::read_str(uc, a.r[1], &s);
    std::string out(s.substr(0, a.r[2]));
    out.resize(a.r[2], '\0');
    emu::write(uc, a.r[0], out.data(), out.size());
    *rv = a.r[0];
}
static void h_strcat(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string d, s; emu::read_str(uc, a.r[0], &d); emu::read_str(uc, a.r[1], &s);
    d += s;
    emu::write(uc, a.r[0], d.c_str(), d.size() + 1);
    *rv = a.r[0];
}
static void h_strncat(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string d, s; emu::read_str(uc, a.r[0], &d);
    emu::read_str(uc, a.r[1], &s, a.r[2]);
    d += s.substr(0, a.r[2]);
    emu::write(uc, a.r[0], d.c_str(), d.size() + 1);
    *rv = a.r[0];
}
static void h_strcmp(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string x, y; emu::read_str(uc, a.r[0], &x); emu::read_str(uc, a.r[1], &y);
    *rv = (uint32_t)strcmp(x.c_str(), y.c_str());
}
static void h_strncmp(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string x, y; emu::read_str(uc, a.r[0], &x, a.r[2]); emu::read_str(uc, a.r[1], &y, a.r[2]);
    *rv = (uint32_t)strncmp(x.c_str(), y.c_str(), a.r[2]);
}
static void h_strchr(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string s; emu::read_str(uc, a.r[0], &s);
    const char *p = strchr(s.c_str(), (char)a.r[1]);
    *rv = p ? a.r[0] + (uint32_t)(p - s.c_str()) : 0;
}
static void h_strstr(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string h, n; emu::read_str(uc, a.r[0], &h); emu::read_str(uc, a.r[1], &n);
    const char *p = strstr(h.c_str(), n.c_str());
    *rv = p ? a.r[0] + (uint32_t)(p - h.c_str()) : 0;
}
static void h_strdup(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string s; emu::read_str(uc, a.r[0], &s);
    *rv = emu::intern_string(uc, s);
}

static void h_malloc(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    *rv = emu::heap_alloc_tagged(uc, a.r[0], "engine-malloc");
    if ((a.r[0] >= 640 && a.r[0] <= 656) || a.r[0] == 168 || a.r[0] == 328 ||
        a.r[0] == 1288 || a.r[0] == 2568 || a.r[0] == 5128 || a.r[0] == 10248 ||
        a.r[0] == 88) {
        uint32_t lr = 0, sp = 0, stk[4] = {0, 0, 0, 0};
        uc_reg_read(uc, UC_ARM_REG_LR, &lr);
        uc_reg_read(uc, UC_ARM_REG_SP, &sp);
        uc_mem_read(uc, sp, stk, 16);
        __android_log_print(ANDROID_LOG_INFO, "zcr-heap",
                            "[alloc648] size=%u ptr=0x%x lr=0x%x sp=0x%x stack=%08x %08x %08x %08x",
                            a.r[0], *rv, lr, sp, stk[0], stk[1], stk[2], stk[3]);
    }
}
static void h_free(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    emu::heap_free(uc, a.r[0]);
    *rv = 0;
}
static void h_realloc(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    *rv = emu::heap_realloc(uc, a.r[0], a.r[1]);
}

static void h_snprintf(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string fmt; emu::read_str(uc, a.r[2], &fmt);
    std::string out;
    format_into(uc, a, 3, fmt, &out);
    size_t cap = a.r[1];
    if (cap == 0) { *rv = (uint32_t)out.size(); return; }
    std::string w = out.substr(0, cap - 1);
    w.push_back('\0');
    emu::write(uc, a.r[0], w.data(), w.size());
    *rv = (uint32_t)out.size();
}
static void h_sprintf(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string fmt; emu::read_str(uc, a.r[1], &fmt);
    std::string out;
    format_into(uc, a, 2, fmt, &out);
    emu::write(uc, a.r[0], out.c_str(), out.size() + 1);
    *rv = (uint32_t)out.size();
}
static void h_printf(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string fmt; emu::read_str(uc, a.r[0], &fmt);
    std::string out;
    format_into(uc, a, 1, fmt, &out);
    fputs(out.c_str(), stdout);
    *rv = (uint32_t)out.size();
}
static void h_fprintf(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string fmt; emu::read_str(uc, a.r[1], &fmt);
    std::string out;
    format_into(uc, a, 2, fmt, &out);
    FILE *f = file_get(a.r[0]);
    if (f) fputs(out.c_str(), f);
    else __android_log_print(ANDROID_LOG_INFO, TAG, "%s", out.c_str());
    *rv = (uint32_t)out.size();
}
static void h_puts(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string s; emu::read_str(uc, a.r[0], &s);
    puts(s.c_str());
    *rv = (uint32_t)s.size() + 1;
}
static void h_putchar(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    putchar((int)a.r[0]);
    *rv = a.r[0];
}
static void h_sscanf(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string src, fmt; emu::read_str(uc, a.r[0], &src); emu::read_str(uc, a.r[1], &fmt);
    /* support the simple "%d %f %s" cases the engine uses */
    std::vector<uint32_t> outp;
    unsigned idx = 2;
    int n = 0;
    size_t si = 0, fi = 0;
    while (fi < fmt.size()) {
        if (fmt[fi] != '%') { fi++; continue; }
        fi++;
        while (fi < fmt.size() && strchr("0123456789.*hl", fmt[fi])) fi++;
        if (fi >= fmt.size()) break;
        char t = fmt[fi++];
        uint32_t p = a.arg(idx++);
        if (!p) break;
        if (t == 'd') {
            int v = 0;
            sscanf(src.c_str() + si, "%d", &v);
            emu::write(uc, p, &v, 4);
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "%d", v);
            si += strlen(tmp);
            n++;
        } else if (t == 'f') {
            float v = 0;
            sscanf(src.c_str() + si, "%f", &v);
            emu::write(uc, p, &v, 4);
            n++;
            si = src.size();
        } else if (t == 's') {
            char buf[256] = {0};
            sscanf(src.c_str() + si, "%255s", buf);
            emu::write(uc, p, buf, strlen(buf) + 1);
            si += strlen(buf);
            n++;
        }
    }
    *rv = (uint32_t)n;
}

static void h_fopen(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string path, mode;
    emu::read_str(uc, a.r[0], &path);
    emu::read_str(uc, a.r[1], &mode);
    FILE *f = fopen(path.c_str(), mode.c_str());
    if (!f) {
        /* engine often stores saves under its private dir; try relative too */
        f = fopen(path.c_str(), mode.c_str());
    }
    *rv = f ? file_token(f) : 0;
}
static void h_fclose(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    FILE *f = file_get(a.r[0]);
    *rv = f ? (uint32_t)fclose(f) : 0xFFFFFFFF;
}
static void h_fread(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    FILE *f = file_get(a.r[3]);
    if (!f) { *rv = 0; return; }
    size_t n = (size_t)a.r[1] * a.r[2];
    std::vector<uint8_t> buf(n);
    size_t got = fread(buf.data(), 1, n, f);
    emu::write(uc, a.r[0], buf.data(), got);
    *rv = a.r[1] ? (uint32_t)(got / a.r[1]) : 0;
}
static void h_fwrite(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    FILE *f = file_get(a.r[3]);
    size_t n = (size_t)a.r[1] * a.r[2];
    std::vector<uint8_t> buf(n);
    emu::read(uc, a.r[0], buf.data(), n);
    *rv = f ? (uint32_t)(fwrite(buf.data(), 1, n, f) / (a.r[1] ? a.r[1] : 1)) : 0;
}
static void h_fseek(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    FILE *f = file_get(a.r[0]);
    *rv = f ? (uint32_t)fseek(f, (long)a.r[1], (int)a.r[2]) : 0xFFFFFFFF;
}
static void h_ftell(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    FILE *f = file_get(a.r[0]);
    *rv = f ? (uint32_t)ftell(f) : 0xFFFFFFFF;
}
static void h_fflush(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    FILE *f = a.r[0] ? file_get(a.r[0]) : stdout;
    *rv = f ? (uint32_t)fflush(f) : 0;
}

static void h_getenv(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string k; emu::read_str(uc, a.r[0], &k);
    const char *v = getenv(k.c_str());
    *rv = v ? emu::intern_string(uc, v) : 0;
}
static void h_gettimeofday(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    uint32_t buf[4] = {(uint32_t)tv.tv_sec, (uint32_t)tv.tv_usec, 0, 0};
    if (a.r[0]) emu::write(uc, a.r[0], buf, 8);
    *rv = 0;
}
static void h_gmtime(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    time_t t = (time_t)a.r[0];
    struct tm *tm = gmtime(&t);
    static uint32_t slot = 0;
    if (!slot) slot = emu::heap_alloc(uc, 64);
    int32_t f[9] = {tm->tm_sec, tm->tm_min, tm->tm_hour, tm->tm_mday,
                    tm->tm_mon, tm->tm_year, tm->tm_wday, tm->tm_yday, tm->tm_isdst};
    emu::write(uc, slot, f, sizeof(f));
    *rv = slot;
}
static void h_srand48(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    srand(a.r[0]);
    *rv = 0;
}
static void h_lrand48(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    *rv = (uint32_t)(rand() & 0x7FFFFFFF);
}
static void h_arc4random(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    *rv = ((uint32_t)rand() << 16) ^ (uint32_t)rand();
}
static void h_atoi(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string s; emu::read_str(uc, a.r[0], &s);
    *rv = (uint32_t)atoi(s.c_str());
}
static void h_strtod(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string s; emu::read_str(uc, a.r[0], &s);
    double d = strtod(s.c_str(), nullptr);
    float f = (float)d;
    memcpy(rv, &f, 4);
}

static void h_qsort(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    uint32_t base = a.r[0], n = a.r[1], sz = a.r[2], cmp = a.r[3];
    if (!n || !sz || n > 100000) { *rv = 0; return; }
    std::vector<uint8_t> buf((size_t)n * sz);
    emu::read(uc, base, buf.data(), buf.size());
    auto call_cmp = [&](uint8_t *x, uint8_t *y) -> int {
        uint32_t px = emu::heap_alloc(uc, sz), py = emu::heap_alloc(uc, sz);
        emu::write(uc, px, x, sz);
        emu::write(uc, py, y, sz);
        emu::Args ca;
        ca.uc = uc;
        ca.r[0] = px;
        ca.r[1] = py;
        uc_reg_read(uc, UC_ARM_REG_SP, &ca.sp);
        uint32_t lr = 0, pc = cmp, ret = 0, old_lr = 0, old_pc = 0;
        uc_reg_read(uc, UC_ARM_REG_LR, &old_lr);
        uc_reg_read(uc, UC_ARM_REG_PC, &old_pc);
        lr = EMU_RETURN_ADDR + 4;
        uc_reg_write(uc, UC_ARM_REG_R0, &px);
        uc_reg_write(uc, UC_ARM_REG_R1, &py);
        uc_reg_write(uc, UC_ARM_REG_LR, &lr);
        uc_emu_start(uc, cmp, EMU_RETURN_ADDR + 4, 0, 0);
        uc_reg_read(uc, UC_ARM_REG_R0, &ret);
        uc_reg_write(uc, UC_ARM_REG_LR, &old_lr);
        emu::heap_free(uc, px);
        emu::heap_free(uc, py);
        return (int)ret;
    };
    /* insertion sort – the engine sorts small arrays only */
    for (uint32_t i = 1; i < n; i++) {
        for (uint32_t j = i; j > 0; j--) {
            uint8_t *x = buf.data() + (size_t)j * sz;
            uint8_t *y = buf.data() + (size_t)(j - 1) * sz;
            if (call_cmp(x, y) < 0) {
                std::vector<uint8_t> tmp(sz);
                memcpy(tmp.data(), x, sz);
                memcpy(x, y, sz);
                memcpy(y, tmp.data(), sz);
            } else break;
        }
    }
    emu::write(uc, base, buf.data(), buf.size());
    *rv = 0;
}

static void h_setjmp(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    uint32_t buf = a.r[0];
    uint32_t regs[16];
    for (int i = 4; i <= 11; i++) uc_reg_read(uc, (uc_arm_reg)(UC_ARM_REG_R0 + i), &regs[i]);
    uc_reg_read(uc, UC_ARM_REG_SP, &regs[13]);
    uc_reg_read(uc, UC_ARM_REG_LR, &regs[14]);
    regs[15] = 0;
    emu::write(uc, buf, &regs[4], 48);
    *rv = 0;
}
static void h_longjmp(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    uint32_t buf = a.r[0], val = a.r[1] ? a.r[1] : 1;
    uint32_t regs[12];
    emu::read(uc, buf, regs, 48);
    for (int i = 0; i < 8; i++)
        uc_reg_write(uc, (uc_arm_reg)(UC_ARM_REG_R4 + i), &regs[i]);
    uc_reg_write(uc, UC_ARM_REG_SP, &regs[8]);
    uint32_t ret = regs[9];
    uc_reg_write(uc, UC_ARM_REG_LR, &ret);
    *rv = val;
}

static void h_exit(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    __android_log_print(ANDROID_LOG_ERROR, TAG, "engine called exit(%u)", a.r[0]);
    *rv = 0;
}
static void h_abort(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    __android_log_print(ANDROID_LOG_ERROR, TAG, "engine called abort()");
    *rv = 0;
}
static void h_noop(uc_engine *uc, const emu::Args &a, uint32_t *rv) { *rv = 0; }
static void h_one(uc_engine *uc, const emu::Args &a, uint32_t *rv) { *rv = 1; }

#define MATH1(name, fn, type)                                        \
    static void h_##name(uc_engine *uc, const emu::Args &a,          \
                         uint32_t *rv) {                             \
        (void)uc; type x; memcpy(&x, &a.r[0], sizeof(type));         \
        type y = fn(x);                                              \
        memcpy(rv, &y, sizeof(type));                                \
    }
#define MATH2(name, fn)                                              \
    static void h_##name(uc_engine *uc, const emu::Args &a,          \
                         uint32_t *rv) {                             \
        (void)uc; float x, y; memcpy(&x, &a.r[0], 4);                \
        memcpy(&y, &a.r[1], 4); float z = fn(x, y);                  \
        memcpy(rv, &z, 4);                                           \
    }

MATH1(sqrt, sqrtf, float)
MATH1(sqrtf, sqrtf, float)
MATH1(sinf, sinf, float)
MATH1(cosf, cosf, float)
MATH1(floorf, floorf, float)
MATH1(ceil, ceilf, float)
MATH2(pow, powf)
static void h_sin(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc; double x; memcpy(&x, &a.r[0], 8); double y = sin(x); memcpy(rv, &y, 8);
}
static void h_cos(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc; double x; memcpy(&x, &a.r[0], 8); double y = cos(x); memcpy(rv, &y, 8);
}
static void h_tan(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc; double x; memcpy(&x, &a.r[0], 8); double y = tan(x); memcpy(rv, &y, 8);
}
static void h_acos(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    (void)uc; double x; memcpy(&x, &a.r[0], 8); double y = acos(x); memcpy(rv, &y, 8);
}

/* ---- zlib ---- */
static void h_inflateInit_(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    z_stream *z = (z_stream *)calloc(1, sizeof(z_stream));
    std::lock_guard<std::mutex> lk(g_zmu);
    g_zstreams[a.r[0]] = z;
    *rv = (uint32_t)inflateInit_(z, (const char *)"1.2.11", sizeof(z_stream));
}
static void h_inflate(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    z_stream *z = zget(uc, a.r[0]);
    if (!z) { *rv = 0xFFFFFFFFu; return; }
    uint32_t in_ptr, in_len, out_ptr, out_len;
    emu::read(uc, a.r[0] + 0, &in_ptr, 4);
    emu::read(uc, a.r[0] + 4, &in_len, 4);
    emu::read(uc, a.r[0] + 12, &out_ptr, 4);
    emu::read(uc, a.r[0] + 16, &out_len, 4);
    std::vector<uint8_t> in(in_len ? in_len : 1), out(out_len ? out_len : 1);
    if (in_len) emu::read(uc, in_ptr, in.data(), in_len);
    z->next_in = in.data();
    z->avail_in = in_len;
    z->next_out = out.data();
    z->avail_out = out_len;
    int r = inflate(z, (int)a.r[1]);
    uint32_t out_used = out_len - z->avail_out;
    if (out_used) emu::write(uc, out_ptr, out.data(), out_used);
    zwrite_back(uc, a.r[0], z, in_ptr, in_len, out_ptr, out_len);
    *rv = (uint32_t)r;
}
static void h_inflateEnd(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::lock_guard<std::mutex> lk(g_zmu);
    auto it = g_zstreams.find(a.r[0]);
    if (it != g_zstreams.end()) {
        inflateEnd(it->second);
        free(it->second);
        g_zstreams.erase(it);
    }
    *rv = 0;
}
static void h_inflateReset(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    z_stream *z = zget(uc, a.r[0]);
    *rv = z ? (uint32_t)inflateReset(z) : 0xFFFFFFFFu;
}
static void h_deflateInit2_(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    z_stream *z = (z_stream *)calloc(1, sizeof(z_stream));
    std::lock_guard<std::mutex> lk(g_zmu);
    g_zstreams[a.r[0]] = z;
    *rv = (uint32_t)deflateInit2_(z, (int)a.r[1], (int)a.r[2], (int)a.r[3],
                                  (int)a.arg(4), (int)a.arg(5),
                                  (const char *)"1.2.11", sizeof(z_stream));
}
static void h_deflate(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    z_stream *z = zget(uc, a.r[0]);
    if (!z) { *rv = 0xFFFFFFFFu; return; }
    uint32_t in_ptr, in_len, out_ptr, out_len;
    emu::read(uc, a.r[0] + 0, &in_ptr, 4);
    emu::read(uc, a.r[0] + 4, &in_len, 4);
    emu::read(uc, a.r[0] + 12, &out_ptr, 4);
    emu::read(uc, a.r[0] + 16, &out_len, 4);
    std::vector<uint8_t> in(in_len ? in_len : 1), out(out_len ? out_len : 1);
    if (in_len) emu::read(uc, in_ptr, in.data(), in_len);
    z->next_in = in.data();
    z->avail_in = in_len;
    z->next_out = out.data();
    z->avail_out = out_len;
    int r = deflate(z, (int)a.r[1]);
    uint32_t out_used = out_len - z->avail_out;
    if (out_used) emu::write(uc, out_ptr, out.data(), out_used);
    zwrite_back(uc, a.r[0], z, in_ptr, in_len, out_ptr, out_len);
    *rv = (uint32_t)r;
}
static void h_deflateEnd(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::lock_guard<std::mutex> lk(g_zmu);
    auto it = g_zstreams.find(a.r[0]);
    if (it != g_zstreams.end()) {
        deflateEnd(it->second);
        free(it->second);
        g_zstreams.erase(it);
    }
    *rv = 0;
}
static void h_deflateReset(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    z_stream *z = zget(uc, a.r[0]);
    *rv = z ? (uint32_t)deflateReset(z) : 0xFFFFFFFFu;
}
static void h_crc32(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::vector<uint8_t> buf(a.r[2]);
    if (a.r[2]) emu::read(uc, a.r[1], buf.data(), a.r[2]);
    *rv = (uint32_t)crc32(a.r[0], buf.data(), a.r[2]);
}

static void h_logprint(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string tag, fmt;
    emu::read_str(uc, a.r[1], &tag);
    emu::read_str(uc, a.r[2], &fmt);
    std::string out;
    format_into(uc, a, 3, fmt, &out);
    __android_log_print((int)a.r[0], tag.c_str(), "%s", out.c_str());
    *rv = 0;
}

static void h_remove(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string p; emu::read_str(uc, a.r[0], &p);
    *rv = (uint32_t)remove(p.c_str());
}
static void h_rename(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string x, y; emu::read_str(uc, a.r[0], &x); emu::read_str(uc, a.r[1], &y);
    *rv = (uint32_t)rename(x.c_str(), y.c_str());
}
static void h_unlink(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string p; emu::read_str(uc, a.r[0], &p);
    *rv = (uint32_t)unlink(p.c_str());
}
static void h_access(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string p; emu::read_str(uc, a.r[0], &p);
    *rv = (uint32_t)access(p.c_str(), (int)a.r[1]);
}
static void h_mktemp(uc_engine *uc, const emu::Args &a, uint32_t *rv) {
    std::string p; emu::read_str(uc, a.r[0], &p);
    std::string t = p + "XXXXXX";
    *rv = a.r[0];
}

void register_posix_imports() {
    struct { const char *n; ImportHandler f; } kTab[] = {
        {"memcpy", h_memcpy}, {"memset", h_memset}, {"memmove", h_memmove},
        {"memcmp", h_memcmp}, {"strlen", h_strlen}, {"strcpy", h_strcpy},
        {"strncpy", h_strncpy}, {"strcat", h_strcat}, {"strncat", h_strncat},
        {"strcmp", h_strcmp}, {"strncmp", h_strncmp}, {"strchr", h_strchr},
        {"strstr", h_strstr}, {"strdup", h_strdup}, {"malloc", h_malloc},
        {"free", h_free}, {"realloc", h_realloc}, {"snprintf", h_snprintf},
        {"sprintf", h_sprintf}, {"printf", h_printf}, {"fprintf", h_fprintf},
        {"puts", h_puts}, {"putchar", h_putchar}, {"sscanf", h_sscanf},
        {"fopen", h_fopen}, {"fclose", h_fclose}, {"fread", h_fread},
        {"fwrite", h_fwrite}, {"fseek", h_fseek}, {"ftell", h_ftell},
        {"fflush", h_fflush}, {"getenv", h_getenv},
        {"gettimeofday", h_gettimeofday}, {"gmtime", h_gmtime},
        {"srand48", h_srand48}, {"lrand48", h_lrand48},
        {"arc4random", h_arc4random},         {"atoi", h_atoi}, {"strtod", h_strtod},
        {"setjmp", h_setjmp}, {"longjmp", h_longjmp},
        {"exit", h_exit}, {"abort", h_abort}, {"raise", h_noop},
        {"atexit", h_noop}, {"__aeabi_atexit", h_noop},
        {"__cxa_finalize", h_noop}, {"__cxa_pure_virtual", h_abort},
        {"__gnu_Unwind_Find_exidx", h_noop}, {"__stack_chk_fail", h_abort},
        {"__srget", h_noop}, {"mktemp", h_mktemp}, {"access", h_access},
        {"remove", h_remove}, {"rename", h_rename}, {"unlink", h_unlink},
        {"__android_log_print", h_logprint},
        {"sqrt", h_sqrt}, {"sqrtf", h_sqrtf}, {"sinf", h_sinf},
        {"cosf", h_cosf}, {"floorf", h_floorf}, {"ceil", h_ceil},
        {"pow", h_pow}, {"sin", h_sin}, {"cos", h_cos}, {"tan", h_tan},
        {"acos", h_acos},
        {"inflateInit_", h_inflateInit_}, {"inflate", h_inflate},
        {"inflateEnd", h_inflateEnd}, {"inflateReset", h_inflateReset},
        {"deflateInit2_", h_deflateInit2_}, {"deflate", h_deflate},
        {"deflateEnd", h_deflateEnd}, {"deflateReset", h_deflateReset},
        {"crc32", h_crc32},
    };
    for (auto &e : kTab) stubs_register(e.n, e.f);
    /* qsort calls back into emulated code (its comparator) – must run from
       the call driver, not inside the CPU hook. */
    stubs_register_recursive("qsort", h_qsort);
}
