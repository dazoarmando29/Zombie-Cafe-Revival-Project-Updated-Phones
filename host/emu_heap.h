/* Emulated-heap allocator core: first-fit free list with boundary tags and
 * O(1) coalescing. Pure C++17 (no Android/Unicorn deps) so the exact code
 * running inside the emulator can also be unit-tested on any host.
 *
 * Block layout (all spans 8-aligned):
 *   H+0   uint32 span          total bytes incl. header (8-aligned, >= 16)
 *   H+4   uint32 state         live: kLiveMagic|prev_inuse ; free: kFreeMagic|prev_inuse
 *   H+8   user data            (free blocks: fd at H+8, bk at H+12)
 *   H+S-4 uint32 span          footer, written only while the block is free
 *
 *   state bit0 = kPrevInUse: the block ending at H is allocated.
 *
 * The user pointer returned by alloc() is H+8, so guest-visible behavior
 * (8-byte alignment, usable size >= requested) matches the previous bump
 * allocator exactly. */
#pragma once

#include <cstddef>
#include <cstdint>

namespace emu {

struct HeapStats {
    uint64_t arena_size = 0;
    uint64_t cur_bytes = 0;       /* live user bytes (requested, aligned) */
    uint64_t peak_bytes = 0;
    uint64_t cur_blocks = 0;
    uint64_t peak_blocks = 0;
    uint64_t total_allocs = 0;
    uint64_t total_frees = 0;
    uint64_t alloc_failures = 0;
    uint64_t bad_frees = 0;       /* wild / unaligned / corrupt pointer */
    uint64_t double_frees = 0;
    uint64_t free_bytes = 0;      /* usable bytes in free blocks */
    uint64_t free_blocks = 0;
    uint64_t largest_free = 0;
    uint64_t largest_live = 0;
    uint64_t coalesces = 0;
};

class Heap {
  public:
    /* buf: host buffer of `size` bytes, guest address of buf == guest_base */
    bool init(void *buf, uint32_t guest_base, size_t size);
    void reset();

    uint32_t alloc(size_t n, uint32_t caller_lr = 0);
    /* returns 0 = freed, 1 = bad/corrupt pointer, 2 = double free */
    int free(uint32_t user_addr, uint32_t caller_lr = 0);
    uint32_t realloc(uint32_t user_addr, size_t n, uint32_t caller_lr = 0);

    void stats(HeapStats *out) const;
    /* Human-readable description of the block containing `addr`
     * (guest address), for crash dumps. Returns false if outside the heap. */
    bool describe(uint32_t addr, char *buf, size_t n) const;

    /* Optional logger for warnings (bad frees etc.). */
    void set_logger(void (*fn)(const char *));

  private:
    uint8_t *mem = nullptr;
    uint32_t base = 0;
    size_t arena = 0;
    uint32_t end_sentinel = 0; /* guest addr of the never-allocatable tail */
    uint32_t free_head = 0;    /* guest addr of first free block header, 0 = none */

    mutable HeapStats st;

    void log(const char *msg) const;
    uint8_t *host_of(uint32_t hdr) const { return mem + (hdr - base); }
    uint32_t rd32(uint32_t addr) const;
    void wr32(uint32_t addr, uint32_t v) const;
    void unlink_free(uint32_t hdr);
    void insert_free(uint32_t hdr, uint32_t span);
    void mark_live(uint32_t hdr, uint32_t span, uint32_t prev_in_use);
    void refresh_free_stats() const;
};

}  // namespace emu
