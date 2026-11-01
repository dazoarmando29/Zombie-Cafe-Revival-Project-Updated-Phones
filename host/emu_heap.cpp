#include "emu_heap.h"

#include <cstdio>
#include <cstring>

namespace emu {
namespace {

constexpr uint32_t kFlagPrevInUse = 1u;
constexpr uint32_t kStateMask = ~1u;
constexpr uint32_t kLiveTag = 0xA110C8E0u;
constexpr uint32_t kFreeTag = 0xF4EEB10Cu;
constexpr uint32_t kMinSpan = 16u;
constexpr uint32_t kReserve = 64u; /* bytes reserved at the arena start */

inline bool is_live(uint32_t state) { return (state & kStateMask) == kLiveTag; }
inline bool is_free(uint32_t state) { return (state & kStateMask) == kFreeTag; }

void (*g_logfn)(const char *) = nullptr;

}  // namespace

void Heap::set_logger(void (*fn)(const char *)) { g_logfn = fn; }

void Heap::log(const char *msg) const {
    if (g_logfn) g_logfn(msg);
}

uint32_t Heap::rd32(uint32_t addr) const {
    uint32_t v;
    memcpy(&v, host_of(addr), 4);
    return v;
}

void Heap::wr32(uint32_t addr, uint32_t v) const {
    memcpy(host_of(addr), &v, 4);
}

bool Heap::init(void *buf, uint32_t guest_base, size_t size) {
    if (!buf || size < kReserve + 2 * kMinSpan) return false;
    mem = (uint8_t *)buf;
    base = guest_base;
    arena = size;
    memset(mem, 0, size);

    end_sentinel = guest_base + (uint32_t)size - kMinSpan;
    /* sentinel: permanently-live 16-byte block capping the walk */
    wr32(end_sentinel, kMinSpan);
    wr32(end_sentinel + 4, kLiveTag | kFlagPrevInUse);

    uint32_t hdr = guest_base + kReserve;
    uint32_t span = (uint32_t)size - kReserve - kMinSpan;
    wr32(hdr, span);
    wr32(hdr + 4, kFreeTag | kFlagPrevInUse);
    wr32(hdr + span - 4, span); /* footer */
    wr32(hdr + 8, 0);           /* fd */
    wr32(hdr + 12, 0);          /* bk */
    free_head = hdr;

    memset(&st, 0, sizeof(st));
    st.arena_size = size;
    st.free_bytes = span - 8;
    st.free_blocks = 1;
    st.largest_free = span - 8;
    return true;
}

void Heap::reset() {
    if (mem) init(mem, base, arena);
}

void Heap::unlink_free(uint32_t hdr) {
    uint32_t fd = rd32(hdr + 8), bk = rd32(hdr + 12);
    if (bk) wr32(bk + 8, fd);
    else free_head = fd;
    if (fd) wr32(fd + 12, bk);
}

void Heap::insert_free(uint32_t hdr, uint32_t span) {
    uint32_t prev_in_use = rd32(hdr + 4) & kFlagPrevInUse;
    wr32(hdr, span);
    wr32(hdr + 4, kFreeTag | prev_in_use);
    wr32(hdr + span - 4, span); /* footer for backward coalescing */
    wr32(hdr + 8, free_head);
    wr32(hdr + 12, 0);
    if (free_head) wr32(free_head + 12, hdr);
    free_head = hdr;
}

void Heap::mark_live(uint32_t hdr, uint32_t span, uint32_t prev_in_use) {
    wr32(hdr, span);
    wr32(hdr + 4, kLiveTag | prev_in_use);
}

void Heap::refresh_free_stats() const {
    st.free_blocks = 0;
    st.free_bytes = 0;
    st.largest_free = 0;
    uint32_t hdr = free_head, guard = 0;
    while (hdr && guard++ < 1000000u) {
        uint32_t span = rd32(hdr);
        uint64_t usable = span >= 8 ? span - 8 : 0;
        st.free_blocks++;
        st.free_bytes += usable;
        if (usable > st.largest_free) st.largest_free = usable;
        hdr = rd32(hdr + 8);
    }
}

uint32_t Heap::alloc(size_t n, uint32_t caller_lr) {
    (void)caller_lr;
    if (!mem) return 0;
    if (n == 0) n = 1;
    if (n > 0x4000000) { /* >64MB request = engine bug, fail loudly */
        st.alloc_failures++;
        char buf[64];
        snprintf(buf, sizeof(buf), "REJECTED huge alloc %u bytes", (unsigned)n);
        log(buf);
        return 0;
    }
    uint32_t need = (uint32_t)((n + 7) & ~7ull);
    uint32_t want = need + 8;
    if (want < kMinSpan) want = kMinSpan;

    uint32_t cur = free_head, guard = 0;
    while (cur && guard++ < 1000000u) {
        uint32_t span = rd32(cur);
        if ((span & 7u) || span < kMinSpan) {
            log("free-list corrupt (bad span)");
            break;
        }
        if (span >= want) {
            uint32_t prev_in_use = rd32(cur + 4) & kFlagPrevInUse;
            unlink_free(cur);

            uint32_t give = span;
            if (span - want >= kMinSpan) {
                give = want;
                uint32_t rest = span - want;
                uint32_t rest_hdr = cur + give;
                /* remainder becomes a free block (PREV_INUSE set: we are live) */
                wr32(rest_hdr + 4, kFreeTag | kFlagPrevInUse);
                insert_free(rest_hdr, rest);
            }
            mark_live(cur, give, prev_in_use);
            /* block after the live one: previous (us) is now in use */
            uint32_t next = cur + give;
            if (next <= end_sentinel) wr32(next + 4, rd32(next + 4) | kFlagPrevInUse);

            st.total_allocs++;
            st.cur_blocks++;
            if (st.cur_blocks > st.peak_blocks) st.peak_blocks = st.cur_blocks;
            uint64_t ub = give - 8;
            st.cur_bytes += ub;
            if (st.cur_bytes > st.peak_bytes) st.peak_bytes = st.cur_bytes;
            if (ub > st.largest_live) st.largest_live = ub;
            return cur + 8;
        }
        cur = rd32(cur + 8);
    }

    st.alloc_failures++;
    char buf[128];
    snprintf(buf, sizeof(buf), "EXHAUSTED: want %u bytes", (unsigned)n);
    log(buf);
    return 0;
}

int Heap::free(uint32_t user_addr, uint32_t caller_lr) {
    (void)caller_lr;
    if (!mem || !user_addr) return 0;
    if (user_addr & 7u) {
        st.bad_frees++;
        return 1;
    }
    uint32_t hdr = user_addr - 8;
    if (hdr < base + kReserve || hdr >= end_sentinel) {
        st.bad_frees++;
        return 1;
    }
    uint32_t span = rd32(hdr);
    if ((span & 7u) || span < kMinSpan || hdr + span > end_sentinel) {
        st.bad_frees++;
        return 1;
    }
    uint32_t state = rd32(hdr + 4);
    if (!is_live(state)) {
        if (is_free(state)) {
            st.double_frees++;
            return 2;
        }
        st.bad_frees++;
        return 1;
    }

    uint32_t prev_in_use = state & kFlagPrevInUse;
    uint32_t orig_span = span;

    /* forward coalesce */
    uint32_t next = hdr + span;
    if (next < end_sentinel && is_free(rd32(next + 4))) {
        uint32_t nspan = rd32(next);
        if (!(nspan & 7u) && nspan >= kMinSpan && next + nspan <= end_sentinel) {
            unlink_free(next);
            span += nspan;
            st.coalesces++;
        }
    }

    /* backward coalesce (prev free -> its span is stored in our hdr-4 footer) */
    if (!prev_in_use) {
        uint32_t pspan = rd32(hdr - 4);
        if (!(pspan & 7u) && pspan >= kMinSpan && pspan <= hdr - (base + kReserve)) {
            uint32_t phdr = hdr - pspan;
            if (is_free(rd32(phdr + 4)) && rd32(phdr) == pspan) {
                unlink_free(phdr);
                hdr = phdr;
                span += pspan;
                prev_in_use = rd32(phdr + 4) & kFlagPrevInUse;
                st.coalesces++;
            }
        }
    }

    /* the block after the merged free block: previous is now free */
    uint32_t after = hdr + span;
    if (after <= end_sentinel) wr32(after + 4, rd32(after + 4) & ~kFlagPrevInUse);

    insert_free(hdr, span);

    st.total_frees++;
    if (st.cur_blocks) st.cur_blocks--;
    st.cur_bytes = st.cur_bytes > (orig_span - 8) ? st.cur_bytes - (orig_span - 8) : 0;
    return 0;
}

uint32_t Heap::realloc(uint32_t user_addr, size_t n, uint32_t caller_lr) {
    if (!user_addr) return alloc(n, caller_lr);
    if (!n) {
        free(user_addr, caller_lr);
        return 0;
    }
    if (!mem) return 0;
    uint32_t hdr = user_addr - 8;
    if (hdr < base + kReserve || hdr >= end_sentinel) return 0;
    uint32_t span = rd32(hdr);
    if ((span & 7u) || span < kMinSpan || !is_live(rd32(hdr + 4))) return 0;
    if (span - 8 >= n) return user_addr; /* fits in place */
    uint32_t naddr = alloc(n, caller_lr);
    if (!naddr) return 0;
    memcpy(host_of(naddr), host_of(user_addr), span - 8);
    free(user_addr, caller_lr);
    return naddr;
}

void Heap::stats(HeapStats *out) const {
    refresh_free_stats();
    *out = st;
}

bool Heap::describe(uint32_t addr, char *buf, size_t n) const {
    if (!mem || addr < base + kReserve || addr >= end_sentinel) {
        snprintf(buf, n, "0x%x not in heap", addr);
        return false;
    }
    uint32_t hdr = base + kReserve, guard = 0;
    while (hdr < end_sentinel && guard++ < 2000000u) {
        uint32_t span = rd32(hdr);
        if ((span & 7u) || span < kMinSpan || hdr + span > base + arena) {
            snprintf(buf, n, "0x%x in corrupt heap chain at hdr=0x%x", addr, hdr);
            return false;
        }
        if (addr >= hdr && addr < hdr + span) {
            uint32_t state = rd32(hdr + 4);
            if (is_live(state)) {
                snprintf(buf, n,
                         "0x%x in LIVE block hdr=0x%x span=%u user=%u off=%u",
                         addr, hdr, span, span - 8, addr - (hdr + 8));
            } else if (is_free(state)) {
                snprintf(buf, n,
                         "0x%x in FREE block hdr=0x%x span=%u off=%u fd=0x%x bk=0x%x",
                         addr, hdr, span, addr - (hdr + 8), rd32(hdr + 8),
                         rd32(hdr + 12));
            } else {
                snprintf(buf, n, "0x%x in CORRUPT block hdr=0x%x state=0x%x",
                         addr, hdr, state);
            }
            return true;
        }
        hdr += span;
    }
    snprintf(buf, n, "0x%x not inside any heap block", addr);
    return false;
}

}  // namespace emu
