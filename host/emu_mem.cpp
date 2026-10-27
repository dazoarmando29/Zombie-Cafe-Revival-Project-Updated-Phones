#include "emu_mem.h"

#include <cstring>
#include <map>
#include <mutex>
#include <unordered_map>

#include <unicorn/unicorn.h>

#include "engine_memory.h"

namespace emu {
namespace {

uc_engine *g_uc = nullptr;

struct Region {
    uint32_t addr;
    size_t size;
    uint8_t *host;
};
std::vector<Region> g_regions;

/* ---- heap: first-fit free list over the aliased heap buffer ---- */
uint8_t *g_heap = nullptr;
uint32_t g_heap_bump = HEAP_BASE + 64;
uint32_t g_heap_end = HEAP_BASE + HEAP_SIZE;

struct Block {            // lives at the start of every allocation
    uint32_t size;        // user size
    uint32_t free_next;   // emu addr of next free block header, 0 = none
};
std::mutex g_heap_mu;
uint32_t g_free_head = 0;

uint32_t block_hdr(uint32_t user) { return user - 8; }
uint8_t *block_host(uint32_t hdr) { return g_heap + (hdr - HEAP_BASE); }

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
    if (void *p = host_ptr(uc, addr)) {
        const char *s = (const char *)p;
        size_t len = 0;
        while (len < max && s[len]) len++;
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
    g_heap = (uint8_t *)buffer;
    g_heap_bump = HEAP_BASE + 64;
    g_free_head = 0;
    return map_alias(HEAP_BASE, HEAP_SIZE, buffer, false);
}

uint32_t heap_alloc(uc_engine *uc, size_t n) {
    if (n == 0) n = 1;
    n = (n + 7) & ~7ull;
    std::lock_guard<std::mutex> lk(g_heap_mu);

    /* first fit in free list */
    uint32_t prev = 0, cur = g_free_head;
    while (cur) {
        Block *b = (Block *)block_host(cur);
        if (b->size >= n) {
            uint32_t nxt = b->free_next;
            if (prev) ((Block *)block_host(prev))->free_next = nxt;
            else g_free_head = nxt;
            return cur + 8;
        }
        prev = cur;
        cur = b->free_next;
    }

    if (g_heap_bump + n + 8 > g_heap_end) return 0;
    uint32_t hdr = g_heap_bump;
    g_heap_bump += (uint32_t)(n + 8);
    Block *b = (Block *)block_host(hdr);
    b->size = (uint32_t)n;
    b->free_next = 0;
    return hdr + 8;
}

void heap_free(uc_engine *uc, uint32_t addr) {
    (void)uc;
    if (!addr) return;
    uint32_t hdr = block_hdr(addr);
    if (hdr < HEAP_BASE + 64 || hdr >= g_heap_end) return;
    std::lock_guard<std::mutex> lk(g_heap_mu);
    Block *b = (Block *)block_host(hdr);
    b->free_next = g_free_head;
    g_free_head = hdr;
}

uint32_t heap_realloc(uc_engine *uc, uint32_t addr, size_t n) {
    if (!addr) return heap_alloc(uc, n);
    if (!n) { heap_free(uc, addr); return 0; }
    Block *b = (Block *)block_host(block_hdr(addr));
    if (b->size >= n) return addr;
    uint32_t naddr = heap_alloc(uc, n);
    if (!naddr) return 0;
    memcpy(host_ptr(uc, naddr), host_ptr(uc, addr), b->size);
    heap_free(uc, addr);
    return naddr;
}

uint32_t intern_string(uc_engine *uc, const std::string &s) {
    uint32_t a = heap_alloc(uc, s.size() + 1);
    if (!a) return 0;
    void *p = host_ptr(uc, a);
    if (p) memcpy(p, s.c_str(), s.size() + 1);
    else uc_mem_write(uc, a, s.c_str(), s.size() + 1);
    return a;
}

}  // namespace emu
