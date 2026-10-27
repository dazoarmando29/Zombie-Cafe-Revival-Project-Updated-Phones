/* Import trampoline dispatch.
 *
 * The engine's GOT is filled with 16-byte stubs on STUB_PAGE. Executing one
 * traps into the host. Simple handlers run inline in the hook (fast path);
 * handlers that must call back into emulated code (qsort comparators) are
 * deferred to the call driver so emulation is never nested inside a hook. */
#pragma once

#include <cstdint>
#include <string>

#include "emu_mem.h"

struct uc_struct;
typedef struct uc_struct uc_engine;

using ImportHandler = void (*)(uc_engine *uc, const emu::Args &args,
                              uint32_t *retval);

void stubs_init();
void stubs_register(const std::string &name, ImportHandler fn);
void stubs_register_recursive(const std::string &name, ImportHandler fn);

uint32_t stubs_slot_addr(uint32_t slot);

enum class StubResult {
    kHandled,        /* handler ran inline; PC advanced */
    kNeedsDriver,    /* recursive handler queued; emulation must stop */
    kUnknown,
};

StubResult stubs_dispatch(uc_engine *uc, uint32_t pc);

/* Pop a deferred recursive call (valid after kNeedsDriver). */
void stubs_take_pending(ImportHandler *fn, emu::Args *args);

bool stubs_dispatch_by_name(uc_engine *uc, const char *name,
                            const emu::Args &args, uint32_t *retval);
