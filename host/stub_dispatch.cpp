#include "stub_dispatch.h"

#include <cstdio>
#include <cstring>

#include <unicorn/unicorn.h>

#include "engine_memory.h"
#include "generated/engine_tables.h"

namespace {

struct StubSlot {
    const char *name = nullptr;
    ImportHandler fn = nullptr;
};

StubSlot g_slots[ENGINE_IMPORT_COUNT];
std::unordered_map<std::string, ImportHandler> g_handlers;

}  // namespace

void stubs_init() {
    for (uint32_t i = 0; i < ENGINE_IMPORT_COUNT; i++) {
        g_slots[i].name = kEngineImportNames[i];
    }
}

void stubs_register(const std::string &name, ImportHandler fn) {
    g_handlers[name] = fn;
    for (uint32_t i = 0; i < ENGINE_IMPORT_COUNT; i++) {
        if (name == kEngineImportNames[i]) g_slots[i].fn = fn;
    }
}

uint32_t stubs_slot_addr(uint32_t slot) {
    return STUB_PAGE + slot * STUB_SLOT_SIZE;
}
uint32_t stubs_trap_addr_for_slot(uint32_t slot) {
    return stubs_slot_addr(slot);
}

bool stubs_dispatch_by_name(uc_engine *uc, const char *name, uint32_t args[4],
                            uint32_t *retval) {
    auto it = g_handlers.find(name);
    if (it == g_handlers.end() || !it->second) return false;
    *retval = 0;
    return it->second(uc, args, retval);
}

bool stubs_dispatch(uc_engine *uc, uint32_t pc) {
    if (pc < STUB_PAGE) return false;
    uint32_t slot = (pc - STUB_PAGE) / STUB_SLOT_SIZE;
    if (slot >= ENGINE_IMPORT_COUNT) return false;

    uint32_t regs[4];
    uc_reg_read(uc, UC_ARM_REG_R0, &regs[0]);
    uc_reg_read(uc, UC_ARM_REG_R1, &regs[1]);
    uc_reg_read(uc, UC_ARM_REG_R2, &regs[2]);
    uc_reg_read(uc, UC_ARM_REG_R3, &regs[3]);

    uint32_t lr = 0;
    uc_reg_read(uc, UC_ARM_REG_LR, &lr);

    uint32_t retval = 0;
    bool ok = false;
    if (g_slots[slot].fn) {
        ok = g_slots[slot].fn(uc, regs, &retval);
    }
    if (!ok) {
        fprintf(stderr, "[zcr-host] UNIMPLEMENTED import: %s\n",
                g_slots[slot].name);
    }

    uc_reg_write(uc, UC_ARM_REG_R0, &retval);
    uc_reg_write(uc, UC_ARM_REG_PC, &lr);
    return true;
}
