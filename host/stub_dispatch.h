/* Import trampoline dispatch: the emulated engine calls imported functions
 * through 16-byte stubs on STUB_PAGE. Executing a stub traps into the host,
 * which forwards to a native implementation (libc, zlib, OpenGL ES, ...). */
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

struct uc_struct;
typedef struct uc_struct uc_engine;

/* Handler signature: receives the uc handle so it can read emulated memory.
 * args[0..3] = r0..r3 as the emulated caller set them; extra args live on the
 * emulated stack (readable via uc_mem_read at emu sp). Return value is placed
 * in r0. Returning true means "handled"; false = stub left unimplemented. */
using ImportHandler = bool (*)(uc_engine *uc, uint32_t args[4], uint32_t *retval);

void stubs_init();

/* Register a handler for an imported symbol name. */
void stubs_register(const std::string &name, ImportHandler fn);

/* Fill STUB_PAGE: one slot per engine import, in kEngineImportNames order.
 * Returns the stub address for the given import slot. */
uint32_t stubs_slot_addr(uint32_t slot);

/* Called by the CPU-code hook when PC lands on a stub. Dispatches to the
 * handler registered for that slot and advances LR return. */
bool stubs_dispatch(uc_engine *uc, uint32_t pc);

/* Dispatch by handler name (used for JNIEnv trap slots). */
bool stubs_dispatch_by_name(uc_engine *uc, const char *name, uint32_t args[4],
                            uint32_t *retval);

/* 16-byte stub layout helper shared with the loader. */
uint32_t stubs_trap_addr_for_slot(uint32_t slot);
