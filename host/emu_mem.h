/* Emulated address space manager with zero-copy host aliasing.
 *
 * Regions mapped via uc_mem_map_ptr can be dereferenced directly on the host,
 * which turns every memcpy/strlen/GL buffer read into a plain pointer op. */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct uc_struct;
typedef struct uc_struct uc_engine;

namespace emu {

/* Single engine instance (one game, one emulator). */
void set_engine(uc_engine *uc);
uc_engine *engine();

/* Register a region backed by host memory (zero-copy). */
bool map_alias(uint32_t emu_addr, size_t size, void *host_mem, bool exec);

/* Map the emulated heap onto a host buffer (call once after uc_open). */
bool heap_init(void *buffer);

/* Host pointer for an emulated address, or nullptr if not in an aliased
 * region (caller falls back to uc_mem_read/uc_mem_write). */
void *host_ptr(uc_engine *uc, uint32_t emu_addr);

/* Copy helpers that use host_ptr fast paths. */
bool read(uc_engine *uc, uint32_t addr, void *dst, size_t n);
bool write(uc_engine *uc, uint32_t addr, const void *src, size_t n);
/* Read a NUL-terminated string. max=0 means "no practical bound" (16MB):
 * the engine stores whole text files in single Strings (18KB+). */
bool read_str(uc_engine *uc, uint32_t addr, std::string *out, size_t max = 0);

/* Call-argument access: r0..r3 then the emulated stack. */
struct Args {
    uc_engine *uc = nullptr;
    uint32_t r[4] = {0, 0, 0, 0};
    uint32_t sp = 0;
    uint32_t arg(unsigned i) const;
};

/* Bump allocator over the emulated heap (fast, no host malloc). */
uint32_t heap_alloc(uc_engine *uc, size_t n);
uint32_t heap_alloc_tagged(uc_engine *uc, size_t n, const char *tag);
void heap_free(uc_engine *uc, uint32_t addr);   // simple: no coalescing
uint32_t heap_realloc(uc_engine *uc, uint32_t addr, size_t n);
void heap_stats(uint32_t *used_kb, uint32_t *total_kb, uint32_t *free_blocks);
void heap_dump_stats();
/* Record which emulated caller (LR) allocated - identifies the leak source. */
void heap_note_lr(uint32_t lr, size_t n);
/* Describe the heap block containing an emulated address (for fault dumps):
 * which allocation, its alloc/free caller PCs, live or freed. */
void heap_describe(uint32_t addr, char *buf, size_t n);

/* Stash a host string into emulated memory, return its emulated address. */
uint32_t intern_string(uc_engine *uc, const std::string &s);

}  // namespace emu
