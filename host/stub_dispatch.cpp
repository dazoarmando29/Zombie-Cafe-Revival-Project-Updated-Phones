#include "stub_dispatch.h"

#include <cstdio>
#include <cstring>
#include <unordered_map>

#include <unicorn/unicorn.h>

#include "engine_memory.h"
#include "generated/engine_tables.h"

namespace {

struct StubSlot {
    const char *name = nullptr;
    ImportHandler fn = nullptr;
    bool recursive = false;
};

StubSlot g_slots[ENGINE_IMPORT_COUNT];
std::unordered_map<std::string, ImportHandler> g_handlers;

/* deferred recursive call */
ImportHandler g_pending_fn = nullptr;
emu::Args g_pending_args;

emu::Args fill_args(uc_engine *uc) {
    emu::Args a;
    a.uc = uc;
    uc_reg_read(uc, UC_ARM_REG_R0, &a.r[0]);
    uc_reg_read(uc, UC_ARM_REG_R1, &a.r[1]);
    uc_reg_read(uc, UC_ARM_REG_R2, &a.r[2]);
    uc_reg_read(uc, UC_ARM_REG_R3, &a.r[3]);
    uc_reg_read(uc, UC_ARM_REG_SP, &a.sp);
    return a;
}

}  // namespace

void stubs_init() {
    for (uint32_t i = 0; i < ENGINE_IMPORT_COUNT; i++)
        g_slots[i].name = kEngineImportNames[i];
}

void stubs_register(const std::string &name, ImportHandler fn) {
    g_handlers[name] = fn;
    for (uint32_t i = 0; i < ENGINE_IMPORT_COUNT; i++)
        if (name == kEngineImportNames[i]) {
            g_slots[i].fn = fn;
            g_slots[i].recursive = false;
        }
}

void stubs_register_recursive(const std::string &name, ImportHandler fn) {
    g_handlers[name] = fn;
    for (uint32_t i = 0; i < ENGINE_IMPORT_COUNT; i++)
        if (name == kEngineImportNames[i]) {
            g_slots[i].fn = fn;
            g_slots[i].recursive = true;
        }
}

uint32_t stubs_slot_addr(uint32_t slot) {
    return STUB_PAGE + slot * STUB_SLOT_SIZE;
}

void stubs_take_pending(ImportHandler *fn, emu::Args *args) {
    *fn = g_pending_fn;
    *args = g_pending_args;
    g_pending_fn = nullptr;
}

void emu_set_pc(uc_engine *uc, uint32_t addr) {
    /* bit0 of an ARM code address encodes Thumb state */
    uint32_t cpsr = 0;
    uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    if (addr & 1u)
        cpsr |= 0x20u;   /* T bit */
    else
        cpsr &= ~0x20u;
    uc_reg_write(uc, UC_ARM_REG_CPSR, &cpsr);
    uint32_t pc = addr & ~1u;
    uc_reg_write(uc, UC_ARM_REG_PC, &pc);
}

bool stubs_dispatch_by_name(uc_engine *uc, const char *name,
                            const emu::Args &args, uint32_t *retval) {
    auto it = g_handlers.find(name);
    if (it == g_handlers.end() || !it->second) return false;
    *retval = 0;
    it->second(uc, args, retval);
    return true;
}

StubResult stubs_dispatch(uc_engine *uc, uint32_t pc) {
    if (pc < STUB_PAGE) return StubResult::kUnknown;
    uint32_t slot = (pc - STUB_PAGE) / STUB_SLOT_SIZE;
    if (slot >= ENGINE_IMPORT_COUNT) return StubResult::kUnknown;

    emu::Args args = fill_args(uc);

    if (!g_slots[slot].fn) {
        fprintf(stderr, "[zcr-host] UNIMPLEMENTED import: %s\n",
                g_slots[slot].name);
        uint32_t lr = 0, z = 0;
        uc_reg_read(uc, UC_ARM_REG_LR, &lr);
        uc_reg_write(uc, UC_ARM_REG_R0, &z);
        emu_set_pc(uc, lr);
        return StubResult::kHandled;
    }

    if (g_slots[slot].recursive) {
        g_pending_fn = g_slots[slot].fn;
        g_pending_args = args;
        return StubResult::kNeedsDriver;
    }

    uint32_t retval = 0;
    g_slots[slot].fn(uc, args, &retval);
    /* read LR after the handler: longjmp rewrites it to divert control */
    uint32_t lr = 0;
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);
    uc_reg_write(uc, UC_ARM_REG_R0, &retval);
    emu_set_pc(uc, lr);
    return StubResult::kHandled;
}
