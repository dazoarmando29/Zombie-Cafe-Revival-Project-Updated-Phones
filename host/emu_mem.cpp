#include "emu_mem.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>
#include <unordered_map>

#include <android/log.h>
#include <unicorn/unicorn.h>

#include "emu_heap.h"
#include "engine_memory.h"
#include "diag.h"

namespace emu {
namespace {

uc_engine *g_uc = nullptr;

struct Region {
    uint32_t addr;
    size_t size;
    uint8_t *host;
};
std::vector<Region> g_regions;

/* ---- heap: emu::Heap (first-fit + coalescing) over the aliased buffer ---- */
Heap g_heap;
std::mutex g_heap_mu;

void heap_logger(const char *msg) {
    __android_log_print(ANDROID_LOG_WARN, "zcr-heap", "%s", msg);
}

}  // namespace

void set_engine(uc_engine *uc) { g_uc = uc; }
uc_engine *engine() { return g_uc; }

bool map_alias(uint32_t emu_addr, size_t size, void *host_mem, bool exec) {
    uc_err e = uc_mem_map_ptr(g_uc, emu_addr, (size_t)(size + 0xFFF) & ~0xFFFull,
                             exec ? UC_PROT_ALL : (UC_PROT_READ | UC_PROT_WRITE),
                             host_mem);
    if (e != UC_ERR_OK) return false;
    g_regions.push_back({emu_addr, size, (uint8_t *)host_mem});
    return true;
}

void *host_ptr(uc_engine *uc, uint32_t emu_addr) {
    (void)uc;
    for (const Region &r : g_regions) {
        if (emu_addr >= r.addr && emu_addr < r.addr + r.size)
            return r.host + (emu_addr - r.addr);
    }
    return nullptr;
}

bool read(uc_engine *uc, uint32_t addr, void *dst, size_t n) {
    if (void *p = host_ptr(uc, addr)) {
        memcpy(dst, p, n);
        return true;
    }
    return uc_mem_read(uc, addr, dst, n) == UC_ERR_OK;
}

bool write(uc_engine *uc, uint32_t addr, const void *src, size_t n) {
    if (void *p = host_ptr(uc, addr)) {
        memcpy(p, src, n);
        return true;
    }
    return uc_mem_write(uc, addr, src, n) == UC_ERR_OK;
}

bool read_str(uc_engine *uc, uint32_t addr, std::string *out, size_t max) {
    out->clear();
    /* strings can be long (the UI strings file is 18KB in one string) - never
     * truncate below 16MB unless the caller explicitly asks for a bound */
    if (max == 0 || max > 0x1000000u) max = 0x1000000u;
    if (void *p = host_ptr(uc, addr)) {
        /* stay inside the aliased region so we never touch unmapped host RAM */
        size_t room = max;
        for (const Region &r : g_regions) {
            if (addr >= r.addr && addr < r.addr + r.size) {
                room = r.addr + r.size - addr;
                if (room > max) room = max;
                break;
            }
        }
        const char *s = (const char *)p;
        size_t len = 0;
        while (len < room && s[len]) len++;
        out->assign(s, len);
        return true;
    }
    char buf[256];
    size_t got = 0;
    while (got < max) {
        size_t chunk = sizeof(buf) - 1;
        if (chunk > max - got) chunk = max - got;
        if (uc_mem_read(uc, addr + (uint32_t)got, buf, chunk) != UC_ERR_OK)
            return false;
        for (size_t i = 0; i < chunk; i++) {
            if (!buf[i]) {
                out->append(buf, i);
                return true;
            }
        }
        out->append(buf, chunk);
        got += chunk;
    }
    return true;
}

uint32_t Args::arg(unsigned i) const {
    if (i < 4) return r[i];
    uint32_t v = 0;
    uc_mem_read(uc, sp + (i - 4) * 4, &v, 4);
    return v;
}

/* ---------------- heap ---------------- */

bool heap_init(void *buffer) {
    g_heap.set_logger(heap_logger);
    return g_heap.init(buffer, HEAP_BASE, HEAP_SIZE) &&
           map_alias(HEAP_BASE, HEAP_SIZE, buffer, false);
}

/* ---- allocation accounting (find what eats the heap) ---- */
namespace {
struct AllocStats {
    uint64_t bytes = 0;       /* cumulative allocated */
    uint32_t count = 0;
    uint32_t live = 0;        /* live block count */
    uint64_t live_bytes = 0;  /* live byte total */
};
struct BlockRec {
    uint32_t size = 0;
    uint32_t alloc_lr = 0;
    uint32_t free_lr = 0; /* nonzero once freed */
    std::string tag;
};
std::mutex g_stats_mu;
std::map<std::string, AllocStats> g_stats;
std::map<uint32_t, AllocStats> g_by_lr;
std::map<uint32_t, BlockRec> g_blocks; /* hdr -> record (live and recently freed) */
}  // namespace

static uint32_t read_lr(uc_engine *uc) {
    uint32_t lr = 0;
    if (uc) uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    return lr & ~1u;
}

static void note_alloc(uint32_t hdr, uint32_t n, uint32_t lr, const char *tag) {
    std::lock_guard<std::mutex> ls(g_stats_mu);
    BlockRec rec;
    rec.size = n;
    rec.alloc_lr = lr;
    rec.tag = tag ? tag : "engine";
    g_blocks[hdr] = std::move(rec);
    AllocStats &s = g_stats[tag ? tag : "?"];
    s.bytes += n;
    s.count++;
    s.live++;
    s.live_bytes += n;
    AllocStats &l = g_by_lr[lr];
    l.bytes += n;
    l.count++;
    l.live++;
    l.live_bytes += n;
}

static void note_free(uint32_t hdr, uint32_t lr) {
    std::lock_guard<std::mutex> ls(g_stats_mu);
    auto it = g_blocks.find(hdr);
    if (it != g_blocks.end()) {
        BlockRec &rec = it->second;
        rec.free_lr = lr;
        auto st = g_stats.find(rec.tag);
        if (st != g_stats.end() && st->second.live) {
            st->second.live--;
            st->second.live_bytes -= rec.size;
        }
        auto by = g_by_lr.find(rec.alloc_lr);
        if (by != g_by_lr.end() && by->second.live) {
            by->second.live--;
            by->second.live_bytes -= rec.size;
        }
    }
}

uint32_t heap_alloc(uc_engine *uc, size_t n) {
    std::unique_lock<std::mutex> lk(g_heap_mu);
    uint32_t lr = read_lr(uc);
    uint32_t p = g_heap.alloc(n, lr);
    if (!p) {
        HeapStats st;
        g_heap.stats(&st);
        __android_log_print(ANDROID_LOG_ERROR, "zcr-heap",
                            "EXHAUSTED: want %zu, live %llu KB, free %llu KB / %u KB",
                            n, (unsigned long long)(st.cur_bytes / 1024),
                            (unsigned long long)(st.free_bytes / 1024),
                            HEAP_SIZE / 1024);
        lk.unlock();
        heap_dump_stats();
        return 0;
    }
    note_alloc(p - 8, (uint32_t)((n + 7) & ~7ull), lr, "engine");
    return p;
}

uint32_t heap_alloc_tagged(uc_engine *uc, size_t n, const char *tag) {
    std::unique_lock<std::mutex> lk(g_heap_mu);
    uint32_t lr = read_lr(uc);
    uint32_t p = g_heap.alloc(n, lr);
    if (!p) {
        HeapStats st;
        g_heap.stats(&st);
        __android_log_print(ANDROID_LOG_ERROR, "zcr-heap",
                            "EXHAUSTED: want %zu (%s), live %llu KB, free %llu KB / %u KB",
                            n, tag ? tag : "?",
                            (unsigned long long)(st.cur_bytes / 1024),
                            (unsigned long long)(st.free_bytes / 1024),
                            HEAP_SIZE / 1024);
        lk.unlock();
        heap_dump_stats();
        return 0;
    }
    note_alloc(p - 8, (uint32_t)((n + 7) & ~7ull), lr, tag);
    return p;
}

void heap_free(uc_engine *uc, uint32_t addr) {
    if (!addr) return;
    std::lock_guard<std::mutex> lk(g_heap_mu);
    uint32_t lr = read_lr(uc);
    int r = g_heap.free(addr, lr);
    if (r == 1) {
        __android_log_print(ANDROID_LOG_WARN, "zcr-heap",
                            "[badfree] 0x%x invalid pointer (lr=0x%x)", addr, lr);
        return;
    }
    if (r == 2) {
        std::lock_guard<std::mutex> ls(g_stats_mu);
        auto it = g_blocks.find(addr - 8);
        uint32_t prev_lr = it != g_blocks.end() ? it->second.free_lr : 0;
        __android_log_print(ANDROID_LOG_WARN, "zcr-heap",
                            "[badfree] hdr=0x%x double free lr=0x%x prev_free_lr=0x%x",
                            addr - 8, lr, prev_lr);
        return;
    }
    note_free(addr - 8, lr);
}

uint32_t heap_realloc(uc_engine *uc, uint32_t addr, size_t n) {
    if (!addr) return heap_alloc(uc, n);
    if (!n) {
        heap_free(uc, addr);
        return 0;
    }
    std::unique_lock<std::mutex> lk(g_heap_mu);
    uint32_t lr = read_lr(uc);
    /* read the old size before realloc moves it */
    uint32_t old_size = 0;
    {
        std::lock_guard<std::mutex> ls(g_stats_mu);
        auto it = g_blocks.find(addr - 8);
        if (it != g_blocks.end()) old_size = it->second.size;
    }
    uint32_t naddr = g_heap.realloc(addr, n, lr);
    if (!naddr) {
        lk.unlock();
        heap_dump_stats();
        return 0;
    }
    if (naddr != addr) {
        note_free(addr - 8, lr);
        note_alloc(naddr - 8, (uint32_t)((n + 7) & ~7ull), lr, "engine");
    } else if (old_size != (uint32_t)((n + 7) & ~7ull)) {
        std::lock_guard<std::mutex> ls(g_stats_mu);
        auto it = g_blocks.find(addr - 8);
        if (it != g_blocks.end()) it->second.size = (uint32_t)((n + 7) & ~7ull);
    }
    return naddr;
}

void heap_stats(uint32_t *used_kb, uint32_t *total_kb, uint32_t *free_blocks) {
    std::lock_guard<std::mutex> lk(g_heap_mu);
    HeapStats st;
    g_heap.stats(&st);
    *used_kb = (uint32_t)(st.cur_bytes / 1024);
    *total_kb = HEAP_SIZE / 1024;
    *free_blocks = (uint32_t)st.free_blocks;
}

void heap_stats_ex(HeapStats *out) {
    std::lock_guard<std::mutex> lk(g_heap_mu);
    g_heap.stats(out);
}

void heap_dump_stats() {
    {
        std::lock_guard<std::mutex> lk(g_heap_mu);
        HeapStats st;
        g_heap.stats(&st);
        diag_log("[heapstat] arena=%lluKB live=%lluKB peak=%lluKB blocks=%llu peak_blocks=%llu",
                 (unsigned long long)(st.arena_size / 1024),
                 (unsigned long long)(st.cur_bytes / 1024),
                 (unsigned long long)(st.peak_bytes / 1024),
                 (unsigned long long)st.cur_blocks,
                 (unsigned long long)st.peak_blocks);
        diag_log("[heapstat] allocs=%llu frees=%llu fails=%llu badfrees=%llu doublefrees=%llu coalesces=%llu",
                 (unsigned long long)st.total_allocs,
                 (unsigned long long)st.total_frees,
                 (unsigned long long)st.alloc_failures,
                 (unsigned long long)st.bad_frees,
                 (unsigned long long)st.double_frees,
                 (unsigned long long)st.coalesces);
        diag_log("[heapstat] free=%lluKB free_blocks=%llu largest_free=%llu largest_live=%llu",
                 (unsigned long long)(st.free_bytes / 1024),
                 (unsigned long long)st.free_blocks,
                 (unsigned long long)st.largest_free,
                 (unsigned long long)st.largest_live);
    }
    std::lock_guard<std::mutex> lk(g_stats_mu);
    for (auto &kv : g_stats) {
        if (kv.second.count)
            diag_log("[heapstat] %s: total=%lluKB count=%u live=%u liveKB=%llu",
                     kv.first.c_str(), (unsigned long long)(kv.second.bytes / 1024),
                     kv.second.count, kv.second.live,
                     (unsigned long long)(kv.second.live_bytes / 1024));
    }
    /* top emulated callers by LIVE bytes (what is actually retained) */
    std::vector<std::pair<uint32_t, AllocStats>> v(g_by_lr.begin(), g_by_lr.end());
    std::sort(v.begin(), v.end(),
              [](const auto &a, const auto &b) { return a.second.live_bytes > b.second.live_bytes; });
    for (size_t i = 0; i < v.size() && i < 15; i++)
        diag_log("[heaplr] pc=0x%x total=%lluKB count=%u live=%u liveKB=%llu",
                 v[i].first, (unsigned long long)(v[i].second.bytes / 1024),
                 v[i].second.count, v[i].second.live,
                 (unsigned long long)(v[i].second.live_bytes / 1024));
}

void heap_note_lr(uint32_t lr, size_t n) {
    (void)lr;
    (void)n;
    /* per-LR accounting now happens inside heap_alloc/free */
}

void heap_describe(uint32_t addr, char *buf, size_t n) {
    std::string core;
    {
        std::lock_guard<std::mutex> lk(g_heap_mu);
        char tmp[256];
        g_heap.describe(addr, tmp, sizeof(tmp));
        core = tmp;
    }
    std::lock_guard<std::mutex> lk(g_stats_mu);
    if (addr < HEAP_BASE + 64 || addr >= HEAP_BASE + HEAP_SIZE) {
        snprintf(buf, n, "0x%x not in heap", addr);
        return;
    }
    /* nearest block header at or below addr (provenance metadata) */
    auto it = g_blocks.upper_bound(addr);
    if (it == g_blocks.begin()) {
        snprintf(buf, n, "%s", core.c_str());
        return;
    }
    --it;
    const BlockRec &rec = it->second;
    uint32_t hdr = it->first;
    if (addr < hdr + 8 + rec.size) {
        snprintf(buf, n,
                 "%s | block hdr=0x%x size=%u tag=%s alloc_lr=0x%x free_lr=0x%x off=%u",
                 core.c_str(), hdr, rec.size, rec.tag.c_str(), rec.alloc_lr,
                 rec.free_lr, addr - hdr - 8);
    } else {
        snprintf(buf, n,
                 "%s | past block hdr=0x%x size=%u (freed_lr=0x%x gap=%u)",
                 core.c_str(), hdr, rec.size, rec.free_lr,
                 addr - (hdr + 8 + rec.size));
    }
}

uint32_t intern_string(uc_engine *uc, const std::string &s) {
    uint32_t a = heap_alloc_tagged(uc, s.size() + 1, "jni-string");
    if (!a) return 0;
    void *p = host_ptr(uc, a);
    if (p) memcpy(p, s.c_str(), s.size() + 1);
    else uc_mem_write(uc, a, s.c_str(), s.size() + 1);
    return a;
}

}  // namespace emu
